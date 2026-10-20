/*
 * ============================================================================
 *  rdx_ir.c  —  RDX Intermediate Representation Builder  (v2.0)
 * ============================================================================
 *
 *  Implements the IR Builder API declared in rdx_ir.h.
 *  All allocations use the freestanding arena allocator (arena.h).
 *
 *  This is the HIGH-LEVEL SSA IR that sits between the AST and the
 *  backend (LLVM IR / direct x86-64 codegen).
 * ============================================================================
 */

#include "rdx_ir.h"
#include "arena.h"

/* ============================================================
 *  Internal module arena — all IR objects live here.
 *  Allocated lazily on first use.
 * ============================================================ */
extern u8 *rdx_bump_ptr, *rdx_bump_base;

static void *ir_alloc(usize size) {
    return rdx_alloc_bump(size);
}

static void *ir_alloc_zero(usize size) {
    void *p = rdx_alloc_bump(size);
    if (p) my_memset(p, 0, size);
    return p;
}

/* ============================================================
 *  §1  Module
 * ============================================================ */
IrModule *ir_module_new(const char *name, const char *source_path) {
    IrModule *mod = (IrModule*)ir_alloc_zero(sizeof(IrModule));
    if (!mod) return NULL;
    mod->name        = name;
    mod->source_path = source_path;
    mod->ir_version  = 2;

    /* Pre-allocate function / global / import arrays */
    mod->functions  = (IrFunction*)ir_alloc_zero(sizeof(IrFunction) * 512);
    mod->globals    = (IrGlobal*)ir_alloc_zero(sizeof(IrGlobal) * 256);
    mod->imports    = (IrImport*)ir_alloc_zero(sizeof(IrImport) * 128);
    mod->types      = (IrType*)ir_alloc_zero(sizeof(IrType) * 1024);
    mod->string_pool = (const char**)ir_alloc_zero(sizeof(const char*) * 4096);

    mod->function_count = 0;
    mod->global_count   = 0;
    mod->import_count   = 0;
    mod->type_count     = 0;
    mod->string_count   = 0;
    return mod;
}

/* ============================================================
 *  §2  Function
 * ============================================================ */
IrFunction *ir_func_new(IrModule *mod, const char *name, IrFunctionKind kind) {
    if (!mod || mod->function_count >= 512) return NULL;
    IrFunction *fn = &mod->functions[mod->function_count++];
    my_memset(fn, 0, sizeof(IrFunction));
    fn->name         = name;
    fn->kind         = kind;
    fn->blocks       = (IrBlock*)ir_alloc_zero(sizeof(IrBlock) * 256);
    fn->lifetimes    = (IrLifetime*)ir_alloc_zero(sizeof(IrLifetime) * 128);
    fn->resources    = (IrResource*)ir_alloc_zero(sizeof(IrResource) * 64);
    fn->effects      = (IrEffect*)ir_alloc_zero(sizeof(IrEffect) * 64);
    fn->value_defs   = (IrInstr**)ir_alloc_zero(sizeof(IrInstr*) * 8192);
    fn->params       = (IrParam*)ir_alloc_zero(sizeof(IrParam) * 64);
    fn->block_count  = 0;
    fn->value_count  = 0;
    fn->return_type  = IR_TY_VOID;
    return fn;
}

/* ============================================================
 *  §3  Basic Block
 * ============================================================ */
IrBlock *ir_block_new(IrFunction *fn, const char *label) {
    if (!fn || fn->block_count >= 256) return NULL;
    IrBlock *blk = &fn->blocks[fn->block_count++];
    my_memset(blk, 0, sizeof(IrBlock));
    blk->id            = fn->block_count - 1;
    blk->label         = label;
    blk->instr_capacity = 256;
    blk->instrs        = (IrInstr*)ir_alloc_zero(sizeof(IrInstr) * blk->instr_capacity);
    blk->predecessors  = (IrBlockId*)ir_alloc_zero(sizeof(IrBlockId) * 32);
    blk->successors    = (IrBlockId*)ir_alloc_zero(sizeof(IrBlockId) * 8);
    blk->lifetimes_begin = (IrLifetimeId*)ir_alloc_zero(sizeof(IrLifetimeId) * 16);
    blk->lifetimes_end   = (IrLifetimeId*)ir_alloc_zero(sizeof(IrLifetimeId) * 16);
    blk->idom          = IR_ID_INVALID;
    return blk;
}

/* ============================================================
 *  §4  Instruction Emission
 * ============================================================ */

/* Allocate a fresh value ID in the function */
static IrValueId ir_alloc_value_id(IrFunction *fn, IrInstr *instr) {
    IrValueId id = fn->value_count++;
    if (id < 8192) {
        fn->value_defs[id] = instr;
    }
    return id;
}

IrValueId ir_emit(IrBlock *blk, IrOpCode op,
                   IrTypeId result_type,
                   IrOperand *operands, u32 operand_count,
                   IrLifetimeId lifetime, IrOwnershipState own_state) {
    if (!blk) return IR_ID_INVALID;
    if (blk->instr_count >= blk->instr_capacity) return IR_ID_INVALID;

    IrInstr *instr = &blk->instrs[blk->instr_count++];
    my_memset(instr, 0, sizeof(IrInstr));
    instr->op              = op;
    instr->result_type     = result_type;
    instr->resource_id     = IR_ID_INVALID;
    instr->result_lifetime = lifetime;
    instr->ownership.value_id  = IR_ID_INVALID;
    instr->ownership.state     = own_state;
    instr->ownership.lifetime  = lifetime;

    if (operand_count > 0 && operands) {
        instr->operands = (IrOperand*)ir_alloc(sizeof(IrOperand) * operand_count);
        my_memcpy(instr->operands, operands, sizeof(IrOperand) * operand_count);
        instr->operand_count = operand_count;
    }

    /* Void ops don't produce a value */
    if (op == IR_STORE || op == IR_JUMP || op == IR_BRANCH ||
        op == IR_SWITCH || op == IR_RETURN || op == IR_PANIC ||
        op == IR_CHAN_SEND || op == IR_MUTEX_UNLOCK || op == IR_RESOURCE_RELEASE) {
        instr->result_id = IR_ID_INVALID;
        return IR_ID_INVALID;
    }

    /* We need the parent function to allocate a value ID.
     * For now we use a module-level counter embedded in the block id. */
    instr->result_id = (IrValueId)((blk->id << 16) | (blk->instr_count - 1));
    return instr->result_id;
}

/* ============================================================
 *  §5  Convenience Emitters
 * ============================================================ */

IrValueId ir_emit_const_int(IrBlock *blk, i64 val) {
    IrOperand op;
    op.is_imm    = TRUE;
    op.int_imm   = val;
    return ir_emit(blk, IR_CONST_INT, IR_TY_INT64, &op, 1, IR_ID_INVALID, OWN_OWNED);
}

IrValueId ir_emit_const_flt(IrBlock *blk, double val) {
    IrOperand op;
    op.is_imm    = TRUE;
    op.flt_imm   = val;
    return ir_emit(blk, IR_CONST_FLOAT, IR_TY_FLOAT64, &op, 1, IR_ID_INVALID, OWN_OWNED);
}

IrValueId ir_emit_const_str(IrBlock *blk, const char *str) {
    IrOperand op;
    op.is_imm    = TRUE;
    op.str_imm   = str;
    return ir_emit(blk, IR_CONST_STR, IR_TY_STR, &op, 1, IR_ID_INVALID, OWN_BORROWED);
}

IrValueId ir_emit_add(IrBlock *blk, IrValueId lhs, IrValueId rhs, IrTypeId ty) {
    IrOperand ops[2];
    ops[0].is_imm = FALSE; ops[0].val = lhs;
    ops[1].is_imm = FALSE; ops[1].val = rhs;
    IrOpCode opc = (ty == IR_TY_FLOAT32 || ty == IR_TY_FLOAT64) ? IR_FADD : IR_ADD;
    return ir_emit(blk, opc, ty, ops, 2, IR_ID_INVALID, OWN_OWNED);
}

IrValueId ir_emit_call(IrBlock *blk, IrValueId fn_val,
                        IrValueId *args, u32 argc, IrTypeId ret_ty) {
    u32 total = 1 + argc;
    IrOperand *ops = (IrOperand*)ir_alloc(sizeof(IrOperand) * total);
    ops[0].is_imm = FALSE; ops[0].val = fn_val;
    for (u32 i = 0; i < argc; i++) {
        ops[1 + i].is_imm = FALSE;
        ops[1 + i].val    = args[i];
    }
    return ir_emit(blk, IR_CALL, ret_ty, ops, total, IR_ID_INVALID, OWN_OWNED);
}

void ir_emit_return(IrBlock *blk, IrValueId val) {
    IrOperand op;
    op.is_imm = FALSE;
    op.val    = val;
    ir_emit(blk, IR_RETURN, IR_TY_VOID, &op, 1, IR_ID_INVALID, OWN_MOVED);
}

void ir_emit_branch(IrBlock *blk, IrValueId cond,
                     IrBlockId then_blk, IrBlockId else_blk) {
    IrOperand ops[3];
    ops[0].is_imm  = FALSE; ops[0].val = cond;
    ops[1].is_imm  = TRUE;  ops[1].int_imm = (i64)then_blk;
    ops[2].is_imm  = TRUE;  ops[2].int_imm = (i64)else_blk;
    ir_emit(blk, IR_BRANCH, IR_TY_VOID, ops, 3, IR_ID_INVALID, OWN_OWNED);
    /* Record CFG edge */
    if (blk->succ_count < 8) {
        blk->successors[blk->succ_count++] = then_blk;
        blk->successors[blk->succ_count++] = else_blk;
    }
}

void ir_emit_jump(IrBlock *blk, IrBlockId target) {
    IrOperand op;
    op.is_imm  = TRUE;
    op.int_imm = (i64)target;
    ir_emit(blk, IR_JUMP, IR_TY_VOID, &op, 1, IR_ID_INVALID, OWN_OWNED);
    if (blk->succ_count < 8) {
        blk->successors[blk->succ_count++] = target;
    }
}

/* ============================================================
 *  §6  Resource Tracking
 * ============================================================ */
IrResourceId ir_resource_acquire(IrFunction *fn, IrResourceKind kind,
                                   IrValueId handle, IrLifetimeId lt,
                                   const char *debug_name) {
    if (!fn || fn->resource_count >= 64) return IR_ID_INVALID;
    IrResourceId id = fn->resource_count++;
    IrResource *res = &fn->resources[id];
    res->id            = id;
    res->kind          = kind;
    res->state         = RES_STATE_ACQUIRED;
    res->handle_value  = handle;
    res->lifetime      = lt;
    res->acquire_block = IR_ID_INVALID;
    res->release_block = IR_ID_INVALID;
    res->debug_name    = debug_name;
    return id;
}

void ir_resource_release(IrFunction *fn, IrResourceId res_id,
                          IrBlockId release_block) {
    if (!fn || res_id >= fn->resource_count) return;
    fn->resources[res_id].state         = RES_STATE_RELEASED;
    fn->resources[res_id].release_block = release_block;
}

/* ============================================================
 *  §7  Ownership / Move Helpers
 * ============================================================ */
void ir_mark_moved(IrFunction *fn, IrValueId val, IrBlockId moved_in) {
    if (!fn || val >= fn->value_count) return;
    IrInstr *def = fn->value_defs[val < 8192 ? val : 0];
    if (def) {
        def->ownership.state         = OWN_MOVED;
        def->ownership.moved_in_block = moved_in;
    }
}

void ir_insert_drop(IrBlock *blk, IrValueId val) {
    IrOperand op;
    op.is_imm = FALSE;
    op.val    = val;
    ir_emit(blk, IR_DROP, IR_TY_VOID, &op, 1, IR_ID_INVALID, OWN_DROPPED);
}

/* ============================================================
 *  §8  Validation
 * ============================================================ */
bool ir_module_validate(const IrModule *mod) {
    if (!mod) return FALSE;
    bool ok = TRUE;

    for (u32 fi = 0; fi < mod->function_count; fi++) {
        const IrFunction *fn = &mod->functions[fi];

        /* Check all resources are released */
        for (u32 ri = 0; ri < fn->resource_count; ri++) {
            if (fn->resources[ri].state == RES_STATE_ACQUIRED ||
                fn->resources[ri].state == RES_STATE_LEAKED) {
                rdx_print("[IR VALIDATE] Resource leak in function '");
                if (fn->name) rdx_print(fn->name);
                rdx_print("': resource '");
                if (fn->resources[ri].debug_name) rdx_print(fn->resources[ri].debug_name);
                rdx_print("' was never released\n");
                ok = FALSE;
            }
        }

        /* Check all basic blocks have terminators */
        for (u32 bi = 0; bi < fn->block_count; bi++) {
            const IrBlock *blk = &fn->blocks[bi];
            if (blk->instr_count == 0) continue;
            const IrInstr *last = &blk->instrs[blk->instr_count - 1];
            bool is_terminator =
                last->op == IR_JUMP   || last->op == IR_BRANCH ||
                last->op == IR_SWITCH || last->op == IR_RETURN ||
                last->op == IR_PANIC;
            if (!is_terminator) {
                rdx_print("[IR VALIDATE] Block '");
                if (blk->label) rdx_print(blk->label);
                rdx_print("' in function '");
                if (fn->name) rdx_print(fn->name);
                rdx_print("' missing terminator instruction\n");
                ok = FALSE;
            }
        }
    }

    return ok;
}

/* ============================================================
 *  §9  IR Dump (for debugging)
 * ============================================================ */
static const char *ir_op_name(IrOpCode op) {
    switch (op) {
        case IR_CONST_INT:         return "const.int";
        case IR_CONST_FLOAT:       return "const.flt";
        case IR_CONST_BOOL:        return "const.blo";
        case IR_CONST_STR:         return "const.str";
        case IR_CONST_NULL:        return "const.null";
        case IR_ALLOCA:            return "alloca";
        case IR_HEAP_ALLOC:        return "heap.alloc";
        case IR_ARENA_ALLOC:       return "arena.alloc";
        case IR_LOAD:              return "load";
        case IR_STORE:             return "store";
        case IR_GEP:               return "gep";
        case IR_DROP:              return "drop";
        case IR_MOVE:              return "move";
        case IR_ADD:               return "add";
        case IR_SUB:               return "sub";
        case IR_MUL:               return "mul";
        case IR_DIV:               return "div";
        case IR_REM:               return "rem";
        case IR_NEG:               return "neg";
        case IR_FADD:              return "fadd";
        case IR_FSUB:              return "fsub";
        case IR_FMUL:              return "fmul";
        case IR_FDIV:              return "fdiv";
        case IR_AND:               return "and";
        case IR_OR:                return "or";
        case IR_XOR:               return "xor";
        case IR_SHL:               return "shl";
        case IR_SHR:               return "shr";
        case IR_NOT:               return "not";
        case IR_CMP_EQ:            return "cmp.eq";
        case IR_CMP_NE:            return "cmp.ne";
        case IR_CMP_LT:            return "cmp.lt";
        case IR_CMP_LE:            return "cmp.le";
        case IR_CMP_GT:            return "cmp.gt";
        case IR_CMP_GE:            return "cmp.ge";
        case IR_CAST_INT_TO_FLOAT: return "cast.i2f";
        case IR_CAST_FLOAT_TO_INT: return "cast.f2i";
        case IR_CAST_WIDEN:        return "cast.widen";
        case IR_CAST_TRUNCATE:     return "cast.trunc";
        case IR_BITCAST:           return "bitcast";
        case IR_JUMP:              return "jump";
        case IR_BRANCH:            return "branch";
        case IR_SWITCH:            return "switch";
        case IR_RETURN:            return "return";
        case IR_PANIC:             return "panic";
        case IR_CALL:              return "call";
        case IR_CALL_INDIRECT:     return "call.indirect";
        case IR_TAIL_CALL:         return "tail.call";
        case IR_SPAWN:             return "spawn";
        case IR_AWAIT:             return "await";
        case IR_CHAN_SEND:         return "chan.send";
        case IR_CHAN_RECV:         return "chan.recv";
        case IR_MUTEX_LOCK:        return "mutex.lock";
        case IR_MUTEX_UNLOCK:      return "mutex.unlock";
        case IR_RESOURCE_ACQUIRE:  return "res.acquire";
        case IR_RESOURCE_RELEASE:  return "res.release";
        case IR_COMPTIME:          return "comptime";
        case IR_INLINE_ASM:        return "asm";
        case IR_EXTERN_CALL:       return "extern.call";
        case IR_PHI:               return "phi";
        case IR_UNDEF:             return "undef";
        default:                   return "???";
    }
}

static const char *ir_ty_name(IrTypeId ty) {
    switch (ty) {
        case IR_TY_VOID:    return "void";
        case IR_TY_BOOL:    return "blo";
        case IR_TY_BYTE:    return "byt";
        case IR_TY_INT64:   return "num";
        case IR_TY_FLOAT32: return "flt";
        case IR_TY_FLOAT64: return "dob";
        case IR_TY_PTR:     return "ptr";
        case IR_TY_REF:     return "&T";
        case IR_TY_REF_MUT: return "&mut T";
        case IR_TY_SLICE:   return "[]T";
        case IR_TY_STR:     return "chr";
        case IR_TY_STRUCT:  return "struct";
        case IR_TY_ENUM:    return "enum";
        case IR_TY_FUNCTION:return "fn";
        case IR_TY_RESULT:  return "Result";
        case IR_TY_OPTION:  return "Option";
        default:            return "?";
    }
}

void ir_function_dump(const IrFunction *fn) {
    if (!fn) return;
    rdx_print("fn ");
    if (fn->name) rdx_print(fn->name);
    rdx_print("() -> ");
    rdx_print(ir_ty_name(fn->return_type));
    rdx_print(" {\n");

    for (u32 bi = 0; bi < fn->block_count; bi++) {
        const IrBlock *blk = &fn->blocks[bi];
        rdx_print("  ");
        if (blk->label) rdx_print(blk->label);
        else { rdx_print("bb"); rdx_print_num(bi); }
        rdx_print(":\n");

        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            const IrInstr *instr = &blk->instrs[ii];
            rdx_print("    ");
            if (instr->result_id != IR_ID_INVALID) {
                rdx_print("%");
                rdx_print_num((i64)instr->result_id);
                rdx_print(" : ");
                rdx_print(ir_ty_name(instr->result_type));
                rdx_print(" = ");
            }
            rdx_print(ir_op_name(instr->op));
            for (u32 oi = 0; oi < instr->operand_count; oi++) {
                rdx_print(" ");
                if (instr->operands[oi].is_imm) {
                    if (instr->op == IR_CONST_STR && instr->operands[oi].str_imm) {
                        rdx_print("\"");
                        rdx_print(instr->operands[oi].str_imm);
                        rdx_print("\"");
                    } else {
                        rdx_print_num(instr->operands[oi].int_imm);
                    }
                } else {
                    rdx_print("%");
                    rdx_print_num((i64)instr->operands[oi].val);
                }
            }
            rdx_print("\n");
        }
    }
    rdx_print("}\n\n");
}

void ir_module_dump(const IrModule *mod) {
    if (!mod) return;
    rdx_print("; RDX-IR Module: ");
    if (mod->name) rdx_print(mod->name);
    rdx_print("\n; Source: ");
    if (mod->source_path) rdx_print(mod->source_path);
    rdx_print("\n\n");
    for (u32 i = 0; i < mod->function_count; i++) {
        ir_function_dump(&mod->functions[i]);
    }
}
