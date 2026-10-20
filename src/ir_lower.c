/*
 * ============================================================================
 *  ir_lower.c  —  RDX AST → RDX-IR Lowering Pass Implementation  (v2.0)
 * ============================================================================
 *
 *  Walks the typed, ownership-checked AST and produces RDX-IR SSA form.
 *
 *  Design principles:
 *    - One-pass lowering (no fixups except break/continue jumps)
 *    - Every expression produces exactly one IrValueId
 *    - Scope exit emits IR_DROP for all non-copy values
 *    - If/loop merges produce IR_PHI nodes
 *    - Resources: File.open → IR_RESOURCE_ACQUIRE, scope exit → IR_RESOURCE_RELEASE
 * ============================================================================
 */

#include "ir_lower.h"

/* ============================================================
 *  Helpers
 * ============================================================ */
static void lower_error(LowerCtx *ctx, u32 line, u32 col, const char *msg) {
    ctx->had_error = TRUE;
    rdx_print("[IR LOWER ERROR] Line ");
    rdx_print_num(line);
    rdx_print(" Col ");
    rdx_print_num(col);
    rdx_print(": ");
    rdx_print(msg);
    rdx_print("\n");
}

static void lower_push_scope(LowerCtx *ctx) {
    if (ctx->scope_depth >= LOWER_MAX_SCOPES) return;
    LowerScope *scope = &ctx->scopes[ctx->scope_depth++];
    scope->base     = ctx->local_count;
    scope->count    = 0;
    scope->lifetime = ctx->next_lifetime++;
}

static void lower_pop_scope(LowerCtx *ctx) {
    if (ctx->scope_depth == 0) return;
    ctx->scope_depth--;
    LowerScope *scope = &ctx->scopes[ctx->scope_depth];
    /* Emit IR_DROP for all non-copy values declared in this scope */
    for (u32 i = scope->base; i < ctx->local_count; i++) {
        /* TODO: check if type is copy — for now emit drop for all */
        ir_insert_drop(ctx->current_block, ctx->locals[i].val_id);
    }
    ctx->local_count = scope->base;
}

static void lower_define_local(LowerCtx *ctx, const char *name, usize len,
                                IrValueId val_id, bool is_mut) {
    if (ctx->local_count >= LOWER_MAX_LOCALS) return;
    LowerLocal *loc = &ctx->locals[ctx->local_count++];
    loc->name     = name;
    loc->name_len = len;
    loc->val_id   = val_id;
    loc->is_mut   = is_mut;
    loc->lifetime = ctx->scope_depth > 0 ?
                    ctx->scopes[ctx->scope_depth - 1].lifetime : 0;
    if (ctx->scope_depth > 0) ctx->scopes[ctx->scope_depth - 1].count++;
}

static IrValueId lower_find_local(LowerCtx *ctx, const char *name, usize len) {
    for (i32 i = (i32)ctx->local_count - 1; i >= 0; i--) {
        if (ctx->locals[i].name_len == len &&
            my_strncmp(ctx->locals[i].name, name, len) == 0) {
            return ctx->locals[i].val_id;
        }
    }
    return IR_ID_INVALID;
}

/* Map RdxType to IrTypeId */
static IrTypeId type_to_ir_type(const RdxType *t) {
    if (!t) return IR_TY_VOID;
    switch (t->kind) {
        case TY_NIL:    return IR_TY_VOID;
        case TY_BOOL:   return IR_TY_BOOL;
        case TY_BYTE:   return IR_TY_BYTE;
        case TY_INT:    return IR_TY_INT64;
        case TY_FLOAT:  return IR_TY_FLOAT32;
        case TY_DOUBLE: return IR_TY_FLOAT64;
        case TY_STR:    return IR_TY_STR;
        case TY_PTR:    return IR_TY_PTR;
        case TY_ARRAY:
        case TY_SLICE:  return IR_TY_SLICE;
        case TY_STRUCT: return IR_TY_STRUCT;
        case TY_ENUM:   return IR_TY_ENUM;
        case TY_TUPLE:  return IR_TY_SLICE;  /* represented as slice */
        case TY_FUNCTION:
        case TY_CLOSURE: return IR_TY_FUNCTION;
        case TY_RESULT:  return IR_TY_RESULT;
        case TY_OPTION:  return IR_TY_OPTION;
        default:         return IR_TY_VOID;
    }
}

/* ============================================================
 *  Expression Lowering — forward declaration
 * ============================================================ */
static IrValueId lower_expr(LowerCtx *ctx, AstExpr *expr);
static void      lower_stmt(LowerCtx *ctx, AstStmt *stmt);

/* ============================================================
 *  lower_expr
 * ============================================================ */
static IrValueId lower_expr(LowerCtx *ctx, AstExpr *expr) {
    if (!expr || !ctx->current_block) return IR_ID_INVALID;

    switch (expr->kind) {
        /* --- Literals --- */
        case EXPR_INT:
            return ir_emit_const_int(ctx->current_block, expr->int_val);

        case EXPR_FLOAT:
            return ir_emit_const_flt(ctx->current_block, expr->float_val);

        case EXPR_STR:
            return ir_emit_const_str(ctx->current_block, expr->str_val.chars);

        case EXPR_BOOL: {
            IrOperand op;
            op.is_imm   = TRUE;
            op.int_imm  = expr->bool_val ? 1 : 0;
            return ir_emit(ctx->current_block, IR_CONST_BOOL, IR_TY_BOOL,
                           &op, 1, IR_ID_INVALID, OWN_OWNED);
        }

        case EXPR_NIL: {
            IrOperand op;
            op.is_imm   = TRUE;
            op.int_imm  = 0;
            return ir_emit(ctx->current_block, IR_CONST_NULL, IR_TY_VOID,
                           &op, 1, IR_ID_INVALID, OWN_OWNED);
        }

        /* --- Identifier (variable read) --- */
        case EXPR_IDENT: {
            IrValueId val = lower_find_local(ctx, expr->ident.name, expr->ident.length);
            if (val == IR_ID_INVALID) {
                /* Global / built-in — emit a load from global */
                IrOperand op;
                op.is_imm   = TRUE;
                op.str_imm  = expr->ident.name;
                return ir_emit(ctx->current_block, IR_LOAD, IR_TY_VOID,
                               &op, 1, IR_ID_INVALID, OWN_BORROWED);
            }
            /* Local variable — IR_LOAD from its alloca */
            IrOperand op;
            op.is_imm = FALSE;
            op.val    = val;
            return ir_emit(ctx->current_block, IR_LOAD, IR_TY_VOID,
                           &op, 1, IR_ID_INVALID, OWN_BORROWED);
        }

        /* --- Unary --- */
        case EXPR_UNARY: {
            IrValueId operand = lower_expr(ctx, expr->unary.operand);
            IrOperand op;
            op.is_imm = FALSE;
            op.val    = operand;
            IrOpCode opc = IR_NOT;
            IrTypeId ty  = IR_TY_BOOL;
            switch (expr->unary.op) {
                case TOK_MINUS: opc = IR_NEG; ty = IR_TY_INT64; break;
                case TOK_NOT:   opc = IR_NOT; ty = IR_TY_BOOL; break;
                case TOK_TILDE: opc = IR_NOT; ty = IR_TY_INT64; break;
                default: break;
            }
            return ir_emit(ctx->current_block, opc, ty, &op, 1, IR_ID_INVALID, OWN_OWNED);
        }

        /* --- Binary --- */
        case EXPR_BINARY: {
            /* Short-circuit AND */
            if (expr->binary.op == TOK_AND) {
                IrValueId lhs = lower_expr(ctx, expr->binary.left);
                IrBlock *rhs_block = ir_block_new(ctx->current_fn, "and.rhs");
                IrBlock *end_block = ir_block_new(ctx->current_fn, "and.end");
                ir_emit_branch(ctx->current_block, lhs,
                               rhs_block->id, end_block->id);
                ctx->current_block = rhs_block;
                IrValueId rhs = lower_expr(ctx, expr->binary.right);
                ir_emit_jump(ctx->current_block, end_block->id);
                ctx->current_block = end_block;
                /* phi: if lhs was false, result = false; else result = rhs */
                IrOperand phi_ops[4];
                phi_ops[0].is_imm = FALSE; phi_ops[0].val = lhs;
                phi_ops[1].is_imm = TRUE;  phi_ops[1].int_imm = (i64)(ctx->current_fn->block_count - 3);
                phi_ops[2].is_imm = FALSE; phi_ops[2].val = rhs;
                phi_ops[3].is_imm = TRUE;  phi_ops[3].int_imm = (i64)(rhs_block->id);
                return ir_emit(ctx->current_block, IR_PHI, IR_TY_BOOL,
                               phi_ops, 4, IR_ID_INVALID, OWN_OWNED);
            }

            /* Short-circuit OR */
            if (expr->binary.op == TOK_OR) {
                IrValueId lhs = lower_expr(ctx, expr->binary.left);
                IrBlock *rhs_block = ir_block_new(ctx->current_fn, "or.rhs");
                IrBlock *end_block = ir_block_new(ctx->current_fn, "or.end");
                ir_emit_branch(ctx->current_block, lhs,
                               end_block->id, rhs_block->id);
                ctx->current_block = rhs_block;
                IrValueId rhs = lower_expr(ctx, expr->binary.right);
                ir_emit_jump(ctx->current_block, end_block->id);
                ctx->current_block = end_block;
                IrOperand phi_ops[4];
                phi_ops[0].is_imm = FALSE; phi_ops[0].val = lhs;
                phi_ops[1].is_imm = TRUE;  phi_ops[1].int_imm = (i64)(ctx->current_fn->block_count - 3);
                phi_ops[2].is_imm = FALSE; phi_ops[2].val = rhs;
                phi_ops[3].is_imm = TRUE;  phi_ops[3].int_imm = (i64)(rhs_block->id);
                return ir_emit(ctx->current_block, IR_PHI, IR_TY_BOOL,
                               phi_ops, 4, IR_ID_INVALID, OWN_OWNED);
            }

            IrValueId lhs = lower_expr(ctx, expr->binary.left);
            IrValueId rhs_v = lower_expr(ctx, expr->binary.right);
            IrOperand ops[2];
            ops[0].is_imm = FALSE; ops[0].val = lhs;
            ops[1].is_imm = FALSE; ops[1].val = rhs_v;

            IrOpCode opc = IR_ADD;
            IrTypeId ty  = IR_TY_INT64;
            switch (expr->binary.op) {
                case TOK_PLUS:   opc = IR_ADD;    ty = IR_TY_INT64; break;
                case TOK_MINUS:  opc = IR_SUB;    ty = IR_TY_INT64; break;
                case TOK_STAR:   opc = IR_MUL;    ty = IR_TY_INT64; break;
                case TOK_SLASH:  opc = IR_DIV;    ty = IR_TY_INT64; break;
                case TOK_PERCENT:opc = IR_REM;    ty = IR_TY_INT64; break;
                case TOK_AMP:    opc = IR_AND;    ty = IR_TY_INT64; break;
                case TOK_PIPE:   opc = IR_OR;     ty = IR_TY_INT64; break;
                case TOK_CARET:  opc = IR_XOR;    ty = IR_TY_INT64; break;
                case TOK_SHL:    opc = IR_SHL;    ty = IR_TY_INT64; break;
                case TOK_SHR:    opc = IR_SHR;    ty = IR_TY_INT64; break;
                case TOK_EQ:     opc = IR_CMP_EQ; ty = IR_TY_BOOL;  break;
                case TOK_NEQ:    opc = IR_CMP_NE; ty = IR_TY_BOOL;  break;
                case TOK_LT:     opc = IR_CMP_LT; ty = IR_TY_BOOL;  break;
                case TOK_LTE:    opc = IR_CMP_LE; ty = IR_TY_BOOL;  break;
                case TOK_GT:     opc = IR_CMP_GT; ty = IR_TY_BOOL;  break;
                case TOK_GTE:    opc = IR_CMP_GE; ty = IR_TY_BOOL;  break;
                default: break;
            }
            return ir_emit(ctx->current_block, opc, ty, ops, 2, IR_ID_INVALID, OWN_OWNED);
        }

        /* --- Assignment --- */
        case EXPR_ASSIGN: {
            IrValueId new_val = lower_expr(ctx, expr->assign.value);
            if (expr->assign.target && expr->assign.target->kind == EXPR_IDENT) {
                IrValueId slot = lower_find_local(ctx, expr->assign.target->ident.name,
                                                   expr->assign.target->ident.length);
                if (slot != IR_ID_INVALID) {
                    IrOperand ops[2];
                    ops[0].is_imm = FALSE; ops[0].val = slot;
                    ops[1].is_imm = FALSE; ops[1].val = new_val;
                    ir_emit(ctx->current_block, IR_STORE, IR_TY_VOID,
                            ops, 2, IR_ID_INVALID, OWN_OWNED);
                }
            }
            return new_val;
        }

        /* --- Function Call --- */
        case EXPR_CALL: {
            IrValueId callee = lower_expr(ctx, expr->call.callee);
            IrValueId *args = (IrValueId*)arena_alloc(ctx->arena,
                sizeof(IrValueId) * expr->call.arg_count);
            for (u32 i = 0; i < expr->call.arg_count; i++) {
                args[i] = lower_expr(ctx, expr->call.args[i]);
            }
            return ir_emit_call(ctx->current_block, callee, args,
                                expr->call.arg_count, IR_TY_VOID);
        }

        /* --- Block Expression --- */
        case EXPR_BLOCK: {
            lower_push_scope(ctx);
            for (u32 i = 0; i < expr->block.stmt_count; i++) {
                lower_stmt(ctx, expr->block.stmts[i]);
            }
            IrValueId result = IR_ID_INVALID;
            if (expr->block.result_expr) {
                result = lower_expr(ctx, expr->block.result_expr);
            }
            lower_pop_scope(ctx);
            return result;
        }

        /* --- Member Access --- */
        case EXPR_MEMBER: {
            IrValueId target = lower_expr(ctx, expr->member.target);
            IrOperand ops[2];
            ops[0].is_imm   = FALSE; ops[0].val   = target;
            ops[1].is_imm   = TRUE;  ops[1].str_imm = expr->member.field;
            return ir_emit(ctx->current_block, IR_GEP, IR_TY_VOID,
                           ops, 2, IR_ID_INVALID, OWN_BORROWED);
        }

        /* --- Index Access --- */
        case EXPR_INDEX: {
            IrValueId target = lower_expr(ctx, expr->index.target);
            IrValueId idx    = lower_expr(ctx, expr->index.index);
            IrOperand ops[2];
            ops[0].is_imm = FALSE; ops[0].val = target;
            ops[1].is_imm = FALSE; ops[1].val = idx;
            return ir_emit(ctx->current_block, IR_GEP, IR_TY_VOID,
                           ops, 2, IR_ID_INVALID, OWN_BORROWED);
        }

        /* --- Array Literal --- */
        case EXPR_ARRAY: {
            IrOperand *ops = (IrOperand*)arena_alloc(ctx->arena,
                sizeof(IrOperand) * (expr->array.count + 1));
            ops[0].is_imm    = TRUE;
            ops[0].int_imm   = (i64)expr->array.count;
            for (u32 i = 0; i < expr->array.count; i++) {
                ops[1 + i].is_imm = FALSE;
                ops[1 + i].val    = lower_expr(ctx, expr->array.elements[i]);
            }
            return ir_emit(ctx->current_block, IR_HEAP_ALLOC, IR_TY_SLICE,
                           ops, expr->array.count + 1, IR_ID_INVALID, OWN_OWNED);
        }

        /* --- Spawn --- */
        case EXPR_SPAWN: {
            IrValueId callee = lower_expr(ctx, expr->spawn.call_expr);
            IrOperand op;
            op.is_imm = FALSE;
            op.val    = callee;
            return ir_emit(ctx->current_block, IR_SPAWN, IR_TY_INT64,
                           &op, 1, IR_ID_INVALID, OWN_OWNED);
        }

        /* --- Yield --- */
        case EXPR_YIELD: {
            IrValueId val = IR_ID_INVALID;
            if (expr->yield.value) {
                val = lower_expr(ctx, expr->yield.value);
            }
            IrOperand op;
            op.is_imm = (val == IR_ID_INVALID) ? TRUE : FALSE;
            op.val    = val;
            op.int_imm = 0;
            return ir_emit(ctx->current_block, IR_AWAIT, IR_TY_VOID,
                           &op, 1, IR_ID_INVALID, OWN_OWNED);
        }

        default:
            return IR_ID_INVALID;
    }
}

/* ============================================================
 *  Statement Lowering
 * ============================================================ */
static void lower_stmt(LowerCtx *ctx, AstStmt *stmt) {
    if (!stmt || !ctx->current_block) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            lower_expr(ctx, stmt->expr);
            break;

        case STMT_VAR: {
            IrValueId init_val = IR_ID_INVALID;
            if (stmt->var_decl.init) {
                init_val = lower_expr(ctx, stmt->var_decl.init);
            } else {
                /* Default-initialise to null/zero */
                IrOperand op;
                op.is_imm  = TRUE;
                op.int_imm = 0;
                init_val = ir_emit(ctx->current_block, IR_CONST_NULL, IR_TY_VOID,
                                   &op, 1, IR_ID_INVALID, OWN_OWNED);
            }
            /* Alloca for the variable, then store */
            IrOperand alloca_op;
            alloca_op.is_imm  = TRUE;
            alloca_op.int_imm = 8;  /* assume 8 bytes for now */
            IrValueId slot = ir_emit(ctx->current_block, IR_ALLOCA, IR_TY_PTR,
                                     &alloca_op, 1, IR_ID_INVALID, OWN_OWNED);
            IrOperand store_ops[2];
            store_ops[0].is_imm = FALSE; store_ops[0].val = slot;
            store_ops[1].is_imm = FALSE; store_ops[1].val = init_val;
            ir_emit(ctx->current_block, IR_STORE, IR_TY_VOID,
                    store_ops, 2, IR_ID_INVALID, OWN_OWNED);
            lower_define_local(ctx, stmt->var_decl.name, stmt->var_decl.name_len,
                               slot, stmt->var_decl.is_mut);
            break;
        }

        case STMT_IF: {
            IrValueId cond = lower_expr(ctx, stmt->if_stmt.condition);
            IrBlock *then_block = ir_block_new(ctx->current_fn, "if.then");
            IrBlock *else_block = ir_block_new(ctx->current_fn, "if.else");
            IrBlock *end_block  = ir_block_new(ctx->current_fn, "if.end");

            ir_emit_branch(ctx->current_block, cond,
                           then_block->id, else_block->id);

            /* Then branch */
            ctx->current_block = then_block;
            lower_push_scope(ctx);
            lower_stmt(ctx, stmt->if_stmt.then_branch);
            lower_pop_scope(ctx);
            if (ctx->current_block) ir_emit_jump(ctx->current_block, end_block->id);

            /* Else branch */
            ctx->current_block = else_block;
            if (stmt->if_stmt.else_branch) {
                lower_push_scope(ctx);
                lower_stmt(ctx, stmt->if_stmt.else_branch);
                lower_pop_scope(ctx);
            }
            if (ctx->current_block) ir_emit_jump(ctx->current_block, end_block->id);

            ctx->current_block = end_block;
            break;
        }

        case STMT_WHILE: {
            IrBlock *header_block = ir_block_new(ctx->current_fn, "while.header");
            IrBlock *body_block   = ir_block_new(ctx->current_fn, "while.body");
            IrBlock *exit_block   = ir_block_new(ctx->current_fn, "while.exit");

            ir_emit_jump(ctx->current_block, header_block->id);
            ctx->current_block = header_block;
            IrValueId cond = lower_expr(ctx, stmt->while_stmt.condition);
            ir_emit_branch(ctx->current_block, cond, body_block->id, exit_block->id);

            ctx->current_block = body_block;
            ctx->loop_stack[ctx->loop_depth].break_target = exit_block->id;
            ctx->loop_stack[ctx->loop_depth].cont_target  = header_block->id;
            ctx->loop_depth++;
            lower_push_scope(ctx);
            lower_stmt(ctx, stmt->while_stmt.body);
            lower_pop_scope(ctx);
            ctx->loop_depth--;
            if (ctx->current_block) ir_emit_jump(ctx->current_block, header_block->id);

            ctx->current_block = exit_block;
            break;
        }

        case STMT_LOOP: {
            IrBlock *loop_block = ir_block_new(ctx->current_fn, "loop.body");
            IrBlock *exit_block = ir_block_new(ctx->current_fn, "loop.exit");

            ir_emit_jump(ctx->current_block, loop_block->id);
            ctx->current_block = loop_block;
            ctx->loop_stack[ctx->loop_depth].break_target = exit_block->id;
            ctx->loop_stack[ctx->loop_depth].cont_target  = loop_block->id;
            ctx->loop_depth++;
            lower_push_scope(ctx);
            lower_stmt(ctx, stmt->loop_stmt.body);
            lower_pop_scope(ctx);
            ctx->loop_depth--;
            if (ctx->current_block) ir_emit_jump(ctx->current_block, loop_block->id);

            ctx->current_block = exit_block;
            break;
        }

        case STMT_FOR: {
            /* Loop over iterable: for var in iter { body } */
            IrValueId iter = lower_expr(ctx, stmt->for_stmt.iterable);
            /* Create loop index */
            IrOperand idx_op;
            idx_op.is_imm  = TRUE;
            idx_op.int_imm = 0;
            IrValueId idx_slot = ir_emit(ctx->current_block, IR_ALLOCA, IR_TY_PTR,
                                          &idx_op, 1, IR_ID_INVALID, OWN_OWNED);
            IrValueId idx_init = ir_emit_const_int(ctx->current_block, 0);
            IrOperand store_ops[2];
            store_ops[0].is_imm = FALSE; store_ops[0].val = idx_slot;
            store_ops[1].is_imm = FALSE; store_ops[1].val = idx_init;
            ir_emit(ctx->current_block, IR_STORE, IR_TY_VOID,
                    store_ops, 2, IR_ID_INVALID, OWN_OWNED);

            IrBlock *header = ir_block_new(ctx->current_fn, "for.header");
            IrBlock *body   = ir_block_new(ctx->current_fn, "for.body");
            IrBlock *exit   = ir_block_new(ctx->current_fn, "for.exit");

            ir_emit_jump(ctx->current_block, header->id);
            ctx->current_block = header;
            /* Compare idx < iter.len */
            IrOperand idx_load_op;
            idx_load_op.is_imm = FALSE;
            idx_load_op.val    = idx_slot;
            IrValueId cur_idx = ir_emit(ctx->current_block, IR_LOAD, IR_TY_INT64,
                                        &idx_load_op, 1, IR_ID_INVALID, OWN_BORROWED);
            /* len of iter */
            IrOperand len_ops[2];
            len_ops[0].is_imm   = FALSE; len_ops[0].val = iter;
            len_ops[1].is_imm   = TRUE;  len_ops[1].str_imm = "len";
            IrValueId iter_len = ir_emit(ctx->current_block, IR_GEP, IR_TY_INT64,
                                          len_ops, 2, IR_ID_INVALID, OWN_BORROWED);
            IrOperand cmp_ops[2];
            cmp_ops[0].is_imm = FALSE; cmp_ops[0].val = cur_idx;
            cmp_ops[1].is_imm = FALSE; cmp_ops[1].val = iter_len;
            IrValueId cond = ir_emit(ctx->current_block, IR_CMP_LT, IR_TY_BOOL,
                                      cmp_ops, 2, IR_ID_INVALID, OWN_OWNED);
            ir_emit_branch(ctx->current_block, cond, body->id, exit->id);

            ctx->current_block = body;
            lower_push_scope(ctx);
            /* Bind loop variable = iter[idx] */
            IrOperand elem_ops[2];
            elem_ops[0].is_imm = FALSE; elem_ops[0].val = iter;
            elem_ops[1].is_imm = FALSE; elem_ops[1].val = cur_idx;
            IrValueId elem = ir_emit(ctx->current_block, IR_GEP, IR_TY_VOID,
                                      elem_ops, 2, IR_ID_INVALID, OWN_BORROWED);
            lower_define_local(ctx, stmt->for_stmt.var_name, stmt->for_stmt.var_len,
                               elem, FALSE);
            ctx->loop_stack[ctx->loop_depth].break_target = exit->id;
            ctx->loop_stack[ctx->loop_depth].cont_target  = header->id;
            ctx->loop_depth++;
            lower_stmt(ctx, stmt->for_stmt.body);
            ctx->loop_depth--;
            /* Increment idx */
            IrValueId one = ir_emit_const_int(ctx->current_block, 1);
            IrOperand inc_ops[2];
            inc_ops[0].is_imm = FALSE; inc_ops[0].val = cur_idx;
            inc_ops[1].is_imm = FALSE; inc_ops[1].val = one;
            IrValueId new_idx = ir_emit(ctx->current_block, IR_ADD, IR_TY_INT64,
                                         inc_ops, 2, IR_ID_INVALID, OWN_OWNED);
            IrOperand store2[2];
            store2[0].is_imm = FALSE; store2[0].val = idx_slot;
            store2[1].is_imm = FALSE; store2[1].val = new_idx;
            ir_emit(ctx->current_block, IR_STORE, IR_TY_VOID,
                    store2, 2, IR_ID_INVALID, OWN_OWNED);
            lower_pop_scope(ctx);
            ir_emit_jump(ctx->current_block, header->id);
            ctx->current_block = exit;
            break;
        }

        case STMT_RETURN: {
            IrValueId ret_val = IR_ID_INVALID;
            if (stmt->return_stmt.value) {
                ret_val = lower_expr(ctx, stmt->return_stmt.value);
            } else {
                ret_val = ir_emit_const_int(ctx->current_block, 0);
            }
            ir_emit_return(ctx->current_block, ret_val);
            ctx->current_block = NULL; /* dead block after return */
            break;
        }

        case STMT_BREAK: {
            if (ctx->loop_depth > 0) {
                IrBlockId target = ctx->loop_stack[ctx->loop_depth - 1].break_target;
                ir_emit_jump(ctx->current_block, target);
                ctx->current_block = NULL;
            }
            break;
        }

        case STMT_CONTINUE: {
            if (ctx->loop_depth > 0) {
                IrBlockId target = ctx->loop_stack[ctx->loop_depth - 1].cont_target;
                ir_emit_jump(ctx->current_block, target);
                ctx->current_block = NULL;
            }
            break;
        }

        case STMT_PRINT: {
            IrValueId *args = (IrValueId*)arena_alloc(ctx->arena,
                sizeof(IrValueId) * stmt->print_stmt.count);
            for (u32 i = 0; i < stmt->print_stmt.count; i++) {
                args[i] = lower_expr(ctx, stmt->print_stmt.exprs[i]);
            }
            /* Emit as IR_EXTERN_CALL to rdx_print */
            IrValueId fn_name = ir_emit_const_str(ctx->current_block, "rdx_print");
            ir_emit_call(ctx->current_block, fn_name, args,
                         stmt->print_stmt.count, IR_TY_VOID);
            break;
        }

        default:
            break;
    }
}

/* ============================================================
 *  Function Lowering
 * ============================================================ */
void ir_lower_function(LowerCtx *ctx, AstDecl *decl) {
    if (!decl || decl->kind != DECL_FUNCTION) return;

    IrFunction *fn = ir_func_new(ctx->module, decl->fn_decl.name, FN_NORMAL);
    if (!fn) return;
    fn->is_pub = decl->fn_decl.is_public;

    /* Resolve return type */
    if (decl->fn_decl.return_type && decl->fn_decl.return_type_len > 0) {
        RdxType *ret = tc_find_type(ctx->tc, decl->fn_decl.return_type,
                                     decl->fn_decl.return_type_len);
        fn->return_type = type_to_ir_type(ret);
    }

    /* Create parameters */
    fn->param_count = decl->fn_decl.param_count;
    for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
        fn->params[i].name    = decl->fn_decl.params[i].name;
        fn->params[i].is_mut  = FALSE;
        RdxType *pt = tc_find_type(ctx->tc,
            decl->fn_decl.params[i].type_name,
            decl->fn_decl.params[i].type_len);
        fn->params[i].type_id = type_to_ir_type(pt);
    }

    /* Create entry block */
    IrBlock *entry = ir_block_new(fn, "entry");
    ctx->current_fn    = fn;
    ctx->current_block = entry;

    /* Push function scope and add params as locals */
    lower_push_scope(ctx);
    for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
        IrOperand alloca_op;
        alloca_op.is_imm  = TRUE;
        alloca_op.int_imm = 8;
        IrValueId slot = ir_emit(ctx->current_block, IR_ALLOCA, IR_TY_PTR,
                                  &alloca_op, 1, IR_ID_INVALID, OWN_OWNED);
        fn->params[i].value_id = slot;
        lower_define_local(ctx, decl->fn_decl.params[i].name,
                           decl->fn_decl.params[i].name_len, slot, FALSE);
    }

    /* Lower body */
    if (decl->fn_decl.body) {
        IrValueId result = lower_expr(ctx, decl->fn_decl.body);
        /* If no explicit return, return the body result */
        if (ctx->current_block) {
            if (result == IR_ID_INVALID) {
                result = ir_emit_const_int(ctx->current_block, 0);
            }
            ir_emit_return(ctx->current_block, result);
        }
    }

    lower_pop_scope(ctx);
}

/* ============================================================
 *  Program Lowering — Public Entry Point
 * ============================================================ */
IrModule *ir_lower_program(AstProgram *prog, TypeChecker *tc, Arena *arena) {
    if (!prog || !tc) return NULL;

    IrModule *mod = ir_module_new("rdx_program", "program.rdx");
    if (!mod) return NULL;

    LowerCtx ctx;
    my_memset(&ctx, 0, sizeof(LowerCtx));
    ctx.module        = mod;
    ctx.current_fn    = NULL;
    ctx.current_block = NULL;
    ctx.tc            = tc;
    ctx.next_lifetime = 1;
    ctx.had_error     = FALSE;
    ctx.arena         = arena;

    /* Create a top-level __main__ function for top-level statements */
    IrFunction *main_fn = ir_func_new(mod, "__main__", FN_NORMAL);
    IrBlock *main_entry = ir_block_new(main_fn, "entry");
    ctx.current_fn    = main_fn;
    ctx.current_block = main_entry;
    lower_push_scope(&ctx);

    for (u32 i = 0; i < prog->decl_count; i++) {
        AstDecl *decl = prog->decls[i];
        if (!decl) continue;

        if (decl->kind == DECL_FUNCTION) {
            /* Save/restore context for nested function */
            IrFunction *saved_fn    = ctx.current_fn;
            IrBlock    *saved_block = ctx.current_block;
            u32         saved_lc    = ctx.local_count;
            u32         saved_sd    = ctx.scope_depth;

            ir_lower_function(&ctx, decl);

            ctx.current_fn    = saved_fn;
            ctx.current_block = saved_block;
            ctx.local_count   = saved_lc;
            ctx.scope_depth   = saved_sd;
        } else if (decl->kind == DECL_STMT) {
            lower_stmt(&ctx, decl->stmt);
        }
    }

    /* Terminate main */
    if (ctx.current_block) {
        IrValueId zero = ir_emit_const_int(ctx.current_block, 0);
        ir_emit_return(ctx.current_block, zero);
    }
    lower_pop_scope(&ctx);

    return mod;
}
