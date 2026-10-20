/*
 * ============================================================================
 *  ir_opt.c  —  RDX-IR Optimization Passes Implementation  (v2.0)
 * ============================================================================
 */

#include "ir_opt.h"

/* ============================================================
 *  Internal helpers
 * ============================================================ */
static bool is_const_op(const IrInstr *instr) {
    return instr->op == IR_CONST_INT  ||
           instr->op == IR_CONST_FLOAT||
           instr->op == IR_CONST_BOOL ||
           instr->op == IR_CONST_NULL;
}

static bool operand_is_const(const IrInstr *instr, u32 idx) {
    if (!instr || idx >= instr->operand_count) return FALSE;
    return instr->operands[idx].is_imm;
}

static i64 operand_int(const IrInstr *instr, u32 idx) {
    if (!instr || idx >= instr->operand_count) return 0;
    return instr->operands[idx].int_imm;
}

static void instr_make_const_int(IrInstr *instr, i64 val) {
    instr->op               = IR_CONST_INT;
    instr->result_type      = IR_TY_INT64;
    instr->operand_count    = 1;
    instr->operands[0].is_imm   = TRUE;
    instr->operands[0].int_imm  = val;
}

static void instr_make_const_bool(IrInstr *instr, bool val) {
    instr->op               = IR_CONST_BOOL;
    instr->result_type      = IR_TY_BOOL;
    instr->operand_count    = 1;
    instr->operands[0].is_imm   = TRUE;
    instr->operands[0].int_imm  = val ? 1 : 0;
}

static void instr_make_const_flt(IrInstr *instr, double val) {
    instr->op               = IR_CONST_FLOAT;
    instr->result_type      = IR_TY_FLOAT64;
    instr->operand_count    = 1;
    instr->operands[0].is_imm   = TRUE;
    instr->operands[0].flt_imm  = val;
}

/* ============================================================
 *  Pass 1: Constant Folding
 *
 *  For each instruction with all-immediate operands, evaluate
 *  the operation at compile time and replace with IR_CONST_*.
 * ============================================================ */
OptStats ir_pass_const_fold(IrFunction *fn) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!fn) return stats;

    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];

            /* Binary integer ops with two immediate operands */
            if (instr->operand_count == 2 &&
                operand_is_const(instr, 0) &&
                operand_is_const(instr, 1)) {

                i64 a = operand_int(instr, 0);
                i64 b = operand_int(instr, 1);
                i64 result = 0;
                bool folded = TRUE;

                switch (instr->op) {
                    case IR_ADD: result = a + b; break;
                    case IR_SUB: result = a - b; break;
                    case IR_MUL: result = a * b; break;
                    case IR_DIV:
                        if (b == 0) { folded = FALSE; break; }
                        result = a / b; break;
                    case IR_REM:
                        if (b == 0) { folded = FALSE; break; }
                        result = a % b; break;
                    case IR_AND: result = a & b; break;
                    case IR_OR:  result = a | b; break;
                    case IR_XOR: result = a ^ b; break;
                    case IR_SHL: result = a << b; break;
                    case IR_SHR: result = a >> b; break;
                    case IR_CMP_EQ: instr_make_const_bool(instr, a == b); stats.constants_folded++; folded = FALSE; break;
                    case IR_CMP_NE: instr_make_const_bool(instr, a != b); stats.constants_folded++; folded = FALSE; break;
                    case IR_CMP_LT: instr_make_const_bool(instr, a <  b); stats.constants_folded++; folded = FALSE; break;
                    case IR_CMP_LE: instr_make_const_bool(instr, a <= b); stats.constants_folded++; folded = FALSE; break;
                    case IR_CMP_GT: instr_make_const_bool(instr, a >  b); stats.constants_folded++; folded = FALSE; break;
                    case IR_CMP_GE: instr_make_const_bool(instr, a >= b); stats.constants_folded++; folded = FALSE; break;
                    default: folded = FALSE; break;
                }

                if (folded) {
                    instr_make_const_int(instr, result);
                    stats.constants_folded++;
                }
            }

            /* Float binary ops */
            if (instr->operand_count == 2 &&
                operand_is_const(instr, 0) &&
                operand_is_const(instr, 1) &&
                (instr->result_type == IR_TY_FLOAT32 ||
                 instr->result_type == IR_TY_FLOAT64)) {

                double fa = instr->operands[0].is_imm ?
                    (instr->operands[0].flt_imm != 0.0 ? instr->operands[0].flt_imm : (double)instr->operands[0].int_imm) : 0.0;
                double fb = instr->operands[1].is_imm ?
                    (instr->operands[1].flt_imm != 0.0 ? instr->operands[1].flt_imm : (double)instr->operands[1].int_imm) : 0.0;

                switch (instr->op) {
                    case IR_FADD: instr_make_const_flt(instr, fa + fb); stats.constants_folded++; break;
                    case IR_FSUB: instr_make_const_flt(instr, fa - fb); stats.constants_folded++; break;
                    case IR_FMUL: instr_make_const_flt(instr, fa * fb); stats.constants_folded++; break;
                    case IR_FDIV:
                        if (fb != 0.0) { instr_make_const_flt(instr, fa / fb); stats.constants_folded++; }
                        break;
                    default: break;
                }
            }

            /* Unary constant ops */
            if (instr->operand_count == 1 && operand_is_const(instr, 0)) {
                i64 a = operand_int(instr, 0);
                switch (instr->op) {
                    case IR_NEG: instr_make_const_int(instr, -a); stats.constants_folded++; break;
                    case IR_NOT:
                        if (instr->result_type == IR_TY_BOOL)
                            instr_make_const_bool(instr, a == 0);
                        else
                            instr_make_const_int(instr, ~a);
                        stats.constants_folded++;
                        break;
                    default: break;
                }
            }
        }
    }
    return stats;
}

/* ============================================================
 *  Pass 2: Dead Code Elimination
 *
 *  Marks each SSA value as "used" if it appears as an operand.
 *  Instructions whose result is never used (and have no side effects)
 *  are replaced with IR_UNDEF and later compacted.
 * ============================================================ */
OptStats ir_pass_dce(IrFunction *fn) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!fn) return stats;

    /* Bit-set of used value IDs (supports up to 8192 values) */
    u8 used[1024];  /* 1024 bytes = 8192 bits */
    my_memset(used, 0, sizeof(used));

#define MARK_USED(vid) do { \
    if ((vid) < 8192) { \
        used[(vid) / 8] |= (u8)(1 << ((vid) % 8)); \
    } \
} while(0)

#define IS_USED(vid) (((vid) < 8192) && (used[(vid) / 8] & (1 << ((vid) % 8))))

    /* Mark all operand references as used */
    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];
            for (u32 oi = 0; oi < instr->operand_count; oi++) {
                if (!instr->operands[oi].is_imm) {
                    MARK_USED(instr->operands[oi].val);
                }
            }
        }
    }

    /* Eliminate instructions with unused results and no side effects */
    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];
            if (instr->op == IR_UNDEF) continue;
            if (instr->result_id == IR_ID_INVALID) continue; /* void — keep */
            if (IS_USED(instr->result_id)) continue;

            /* Side-effect-free ops can be removed */
            bool pure = (instr->op == IR_CONST_INT || instr->op == IR_CONST_FLOAT ||
                         instr->op == IR_CONST_BOOL || instr->op == IR_CONST_STR  ||
                         instr->op == IR_ADD || instr->op == IR_SUB ||
                         instr->op == IR_MUL || instr->op == IR_NEG ||
                         instr->op == IR_AND || instr->op == IR_OR  ||
                         instr->op == IR_XOR || instr->op == IR_NOT ||
                         instr->op == IR_CMP_EQ || instr->op == IR_CMP_NE ||
                         instr->op == IR_CMP_LT || instr->op == IR_CMP_LE ||
                         instr->op == IR_CMP_GT || instr->op == IR_CMP_GE ||
                         instr->op == IR_FADD || instr->op == IR_FSUB ||
                         instr->op == IR_FMUL || instr->op == IR_MOVE);
            if (pure) {
                instr->op           = IR_UNDEF;
                instr->result_id    = IR_ID_INVALID;
                instr->operand_count = 0;
                stats.dead_instrs_removed++;
            }
        }
    }

    /* Remove unreachable blocks (blocks with no predecessors except entry) */
    for (u32 bi = 1; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        if (blk->pred_count == 0 && blk->instr_count > 0) {
            /* Clear the block */
            blk->instr_count = 0;
            stats.dead_blocks_removed++;
        }
    }

#undef MARK_USED
#undef IS_USED
    return stats;
}

/* ============================================================
 *  Pass 3: Copy Propagation
 *
 *  If an instruction is just IR_MOVE %a → %b, replace all
 *  uses of %b with %a and mark the MOVE as dead.
 * ============================================================ */
OptStats ir_pass_copy_prop(IrFunction *fn) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!fn) return stats;

    /* Build a renaming map: value_id → canonical_id */
    IrValueId rename[8192];
    for (u32 i = 0; i < 8192; i++) rename[i] = i; /* identity */

    /* Find all MOVE / LOAD(alloca)-of-const patterns */
    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];
            if (instr->op == IR_MOVE && instr->operand_count == 1 &&
                !instr->operands[0].is_imm &&
                instr->result_id != IR_ID_INVALID) {
                IrValueId src = instr->operands[0].val;
                IrValueId dst = instr->result_id;
                if (src < 8192 && dst < 8192) {
                    rename[dst] = rename[src];
                    instr->op = IR_UNDEF;
                    stats.copies_propagated++;
                }
            }
        }
    }

    /* Apply renaming to all operands */
    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];
            for (u32 oi = 0; oi < instr->operand_count; oi++) {
                if (!instr->operands[oi].is_imm) {
                    IrValueId old_val = instr->operands[oi].val;
                    if (old_val < 8192 && rename[old_val] != old_val) {
                        instr->operands[oi].val = rename[old_val];
                    }
                }
            }
        }
    }

    return stats;
}

/* ============================================================
 *  Pass 4: Common Subexpression Elimination (CSE)
 *
 *  Within each basic block, if two instructions perform the same
 *  operation on the same operands, deduplicate them.
 * ============================================================ */
OptStats ir_pass_cse(IrFunction *fn) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!fn) return stats;

    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *a = &blk->instrs[ii];
            if (a->op == IR_UNDEF || a->result_id == IR_ID_INVALID) continue;

            /* Only CSE pure arithmetic ops */
            bool pure = (a->op == IR_ADD || a->op == IR_SUB ||
                         a->op == IR_MUL || a->op == IR_AND ||
                         a->op == IR_OR  || a->op == IR_XOR ||
                         a->op == IR_FADD || a->op == IR_FSUB || a->op == IR_FMUL);
            if (!pure) continue;

            /* Look for a later identical instruction */
            for (u32 jj = ii + 1; jj < blk->instr_count; jj++) {
                IrInstr *b = &blk->instrs[jj];
                if (b->op != a->op) continue;
                if (b->operand_count != a->operand_count) continue;
                if (b->result_id == IR_ID_INVALID) continue;

                bool same = TRUE;
                for (u32 oi = 0; oi < a->operand_count && same; oi++) {
                    if (a->operands[oi].is_imm != b->operands[oi].is_imm) {
                        same = FALSE;
                    } else if (a->operands[oi].is_imm) {
                        same = (a->operands[oi].int_imm == b->operands[oi].int_imm);
                    } else {
                        same = (a->operands[oi].val == b->operands[oi].val);
                    }
                }

                if (same) {
                    /* Replace b with a MOVE from a's result */
                    b->op                  = IR_MOVE;
                    b->operand_count       = 1;
                    b->operands[0].is_imm  = FALSE;
                    b->operands[0].val     = a->result_id;
                    stats.cse_hits++;
                }
            }
        }
    }
    return stats;
}

/* ============================================================
 *  Pass 5: Strength Reduction
 *
 *  Replace expensive ops with cheaper equivalents where safe:
 *    x * 2  →  x + x  (or x << 1)
 *    x * 1  →  x
 *    x + 0  →  x
 *    x - 0  →  x
 *    x * 0  →  0
 *    x / 1  →  x
 * ============================================================ */
OptStats ir_pass_strength_reduce(IrFunction *fn) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!fn) return stats;

    for (u32 bi = 0; bi < fn->block_count; bi++) {
        IrBlock *blk = &fn->blocks[bi];
        for (u32 ii = 0; ii < blk->instr_count; ii++) {
            IrInstr *instr = &blk->instrs[ii];

            if (instr->operand_count != 2) continue;
            bool rhs_imm = operand_is_const(instr, 1);
            bool lhs_imm = operand_is_const(instr, 0);
            i64 rval = rhs_imm ? operand_int(instr, 1) : 0;
            i64 lval = lhs_imm ? operand_int(instr, 0) : 0;

            switch (instr->op) {
                case IR_ADD:
                    if (rhs_imm && rval == 0) {
                        /* x + 0 → x */
                        IrValueId x = instr->operands[0].val;
                        instr->op = IR_MOVE;
                        instr->operand_count = 1;
                        instr->operands[0].is_imm = FALSE;
                        instr->operands[0].val    = x;
                        stats.strength_reductions++;
                    } else if (lhs_imm && lval == 0) {
                        /* 0 + x → x */
                        IrValueId x = instr->operands[1].val;
                        instr->op = IR_MOVE;
                        instr->operand_count = 1;
                        instr->operands[0].is_imm = FALSE;
                        instr->operands[0].val    = x;
                        stats.strength_reductions++;
                    }
                    break;

                case IR_MUL:
                    if (rhs_imm && rval == 0) {
                        /* x * 0 → 0 */
                        instr_make_const_int(instr, 0);
                        stats.strength_reductions++;
                    } else if (rhs_imm && rval == 1) {
                        /* x * 1 → x */
                        IrValueId x = instr->operands[0].val;
                        instr->op = IR_MOVE;
                        instr->operand_count = 1;
                        instr->operands[0].is_imm = FALSE;
                        instr->operands[0].val    = x;
                        stats.strength_reductions++;
                    } else if (rhs_imm && rval == 2) {
                        /* x * 2 → x << 1 */
                        instr->op = IR_SHL;
                        instr->operands[1].int_imm = 1;
                        stats.strength_reductions++;
                    } else if (rhs_imm && rval == 4) {
                        /* x * 4 → x << 2 */
                        instr->op = IR_SHL;
                        instr->operands[1].int_imm = 2;
                        stats.strength_reductions++;
                    } else if (rhs_imm && rval == 8) {
                        /* x * 8 → x << 3 */
                        instr->op = IR_SHL;
                        instr->operands[1].int_imm = 3;
                        stats.strength_reductions++;
                    }
                    break;

                case IR_SUB:
                    if (rhs_imm && rval == 0) {
                        /* x - 0 → x */
                        IrValueId x = instr->operands[0].val;
                        instr->op = IR_MOVE;
                        instr->operand_count = 1;
                        instr->operands[0].is_imm = FALSE;
                        instr->operands[0].val    = x;
                        stats.strength_reductions++;
                    }
                    break;

                case IR_DIV:
                    if (rhs_imm && rval == 1) {
                        /* x / 1 → x */
                        IrValueId x = instr->operands[0].val;
                        instr->op = IR_MOVE;
                        instr->operand_count = 1;
                        instr->operands[0].is_imm = FALSE;
                        instr->operands[0].val    = x;
                        stats.strength_reductions++;
                    }
                    break;

                default:
                    break;
            }
        }
    }
    return stats;
}

/* ============================================================
 *  Pass 6: Function Inlining
 *
 *  For each call site where the callee is known and has <= max_instrs,
 *  inline the function body at the call site.
 *
 *  Simplified: only inline leaf functions (no recursive calls).
 * ============================================================ */
OptStats ir_pass_inline(IrModule *mod, u32 max_inline_instrs) {
    OptStats stats;
    my_memset(&stats, 0, sizeof(OptStats));
    if (!mod) return stats;

    /* Count instructions per function */
    for (u32 fi = 0; fi < mod->function_count; fi++) {
        IrFunction *caller = &mod->functions[fi];
        for (u32 bi = 0; bi < caller->block_count; bi++) {
            IrBlock *blk = &caller->blocks[bi];
            for (u32 ii = 0; ii < blk->instr_count; ii++) {
                IrInstr *instr = &blk->instrs[ii];
                if (instr->op != IR_CALL) continue;
                /* For now: just count — full inlining requires SSA renaming */
                /* TODO: full inlining with value renaming */
            }
        }
    }

    return stats;
}

/* ============================================================
 *  Main optimization pipeline
 * ============================================================ */
static void add_stats(OptStats *total, const OptStats *delta) {
    total->constants_folded    += delta->constants_folded;
    total->dead_instrs_removed += delta->dead_instrs_removed;
    total->dead_blocks_removed += delta->dead_blocks_removed;
    total->copies_propagated   += delta->copies_propagated;
    total->cse_hits            += delta->cse_hits;
    total->functions_inlined   += delta->functions_inlined;
    total->strength_reductions += delta->strength_reductions;
}

OptStats ir_optimize(IrModule *mod, OptLevel level) {
    OptStats total;
    my_memset(&total, 0, sizeof(OptStats));
    if (!mod || level == OPT_LEVEL_NONE) return total;

    /* Run passes over every function */
    for (u32 iter = 0; iter < 3; iter++) {
        bool changed = FALSE;

        for (u32 fi = 0; fi < mod->function_count; fi++) {
            IrFunction *fn = &mod->functions[fi];
            OptStats s;

            /* O1+ */
            s = ir_pass_const_fold(fn);
            add_stats(&total, &s);
            if (s.constants_folded > 0) changed = TRUE;

            s = ir_pass_dce(fn);
            add_stats(&total, &s);
            if (s.dead_instrs_removed > 0) changed = TRUE;

            if (level >= OPT_LEVEL_FULL) {
                s = ir_pass_copy_prop(fn);
                add_stats(&total, &s);
                if (s.copies_propagated > 0) changed = TRUE;

                s = ir_pass_cse(fn);
                add_stats(&total, &s);
                if (s.cse_hits > 0) changed = TRUE;

                s = ir_pass_strength_reduce(fn);
                add_stats(&total, &s);
                if (s.strength_reductions > 0) changed = TRUE;
            }
        }

        if (level >= OPT_LEVEL_FULL) {
            OptStats s = ir_pass_inline(mod, level == OPT_LEVEL_SIZE ? 10 : 50);
            add_stats(&total, &s);
            if (s.functions_inlined > 0) changed = TRUE;
        }

        if (!changed) break; /* Fixed point reached */
    }

    return total;
}

/* ============================================================
 *  Print stats
 * ============================================================ */
void ir_opt_print_stats(const OptStats *stats) {
    if (!stats) return;
    rdx_print("[OPT] Constants folded:     "); rdx_print_num(stats->constants_folded);    rdx_print("\n");
    rdx_print("[OPT] Dead instrs removed:  "); rdx_print_num(stats->dead_instrs_removed); rdx_print("\n");
    rdx_print("[OPT] Dead blocks removed:  "); rdx_print_num(stats->dead_blocks_removed); rdx_print("\n");
    rdx_print("[OPT] Copies propagated:    "); rdx_print_num(stats->copies_propagated);   rdx_print("\n");
    rdx_print("[OPT] CSE hits:             "); rdx_print_num(stats->cse_hits);            rdx_print("\n");
    rdx_print("[OPT] Functions inlined:    "); rdx_print_num(stats->functions_inlined);   rdx_print("\n");
    rdx_print("[OPT] Strength reductions:  "); rdx_print_num(stats->strength_reductions); rdx_print("\n");
}
