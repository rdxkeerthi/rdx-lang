/*
 * ============================================================================
 *  comptime.c  —  RDX Embedded Compile-Time Constant Evaluator Implementation
 * ============================================================================
 */

#include "comptime.h"

void comptime_init(ComptimeContext *ctx, Arena *arena) {
    ctx->arena     = arena;
    ctx->had_error = FALSE;
}

bool comptime_eval_int(ComptimeContext *ctx, AstExpr *expr, i64 *out_val) {
    if (!expr) return FALSE;

    switch (expr->kind) {
        case EXPR_INT:
            *out_val = expr->int_val;
            return TRUE;

        case EXPR_BOOL:
            *out_val = expr->bool_val ? 1 : 0;
            return TRUE;

        case EXPR_UNARY: {
            i64 inner = 0;
            if (!comptime_eval_int(ctx, expr->unary.operand, &inner)) return FALSE;
            switch (expr->unary.op) {
                case TOK_MINUS: *out_val = -inner; return TRUE;
                case TOK_NOT:   *out_val = !inner; return TRUE;
                case TOK_TILDE: *out_val = ~inner; return TRUE;
                default: return FALSE;
            }
        }

        case EXPR_BINARY: {
            i64 left = 0, right = 0;
            if (!comptime_eval_int(ctx, expr->binary.left, &left)) return FALSE;
            if (!comptime_eval_int(ctx, expr->binary.right, &right)) return FALSE;

            switch (expr->binary.op) {
                case TOK_PLUS:    *out_val = left + right; return TRUE;
                case TOK_MINUS:   *out_val = left - right; return TRUE;
                case TOK_STAR:    *out_val = left * right; return TRUE;
                case TOK_SLASH:
                    if (right == 0) return FALSE;
                    *out_val = left / right;
                    return TRUE;
                case TOK_PERCENT:
                    if (right == 0) return FALSE;
                    *out_val = left % right;
                    return TRUE;
                case TOK_AMP:     *out_val = left & right; return TRUE;
                case TOK_PIPE:    *out_val = left | right; return TRUE;
                case TOK_CARET:   *out_val = left ^ right; return TRUE;
                case TOK_SHL:     *out_val = left << right; return TRUE;
                case TOK_SHR:     *out_val = left >> right; return TRUE;
                case TOK_EQ:      *out_val = (left == right) ? 1 : 0; return TRUE;
                case TOK_NEQ:     *out_val = (left != right) ? 1 : 0; return TRUE;
                case TOK_LT:      *out_val = (left < right)  ? 1 : 0; return TRUE;
                case TOK_LTE:     *out_val = (left <= right) ? 1 : 0; return TRUE;
                case TOK_GT:      *out_val = (left > right)  ? 1 : 0; return TRUE;
                case TOK_GTE:     *out_val = (left >= right) ? 1 : 0; return TRUE;
                case TOK_AND:     *out_val = (left && right) ? 1 : 0; return TRUE;
                case TOK_OR:      *out_val = (left || right) ? 1 : 0; return TRUE;
                default: return FALSE;
            }
        }

        case EXPR_BLOCK:
            if (expr->block.result_expr) {
                return comptime_eval_int(ctx, expr->block.result_expr, out_val);
            }
            if (expr->block.stmt_count > 0) {
                AstStmt *last_stmt = expr->block.stmts[expr->block.stmt_count - 1];
                if (last_stmt && last_stmt->kind == STMT_EXPR) {
                    return comptime_eval_int(ctx, last_stmt->expr, out_val);
                }
            }
            return FALSE;

        case EXPR_COMPTIME:
            return comptime_eval_int(ctx, expr->comptime_expr.expr, out_val);

        default:
            return FALSE;
    }
}

bool comptime_eval_float(ComptimeContext *ctx, AstExpr *expr, double *out_val) {
    if (!expr) return FALSE;

    switch (expr->kind) {
        case EXPR_FLOAT:
            *out_val = expr->float_val;
            return TRUE;

        case EXPR_INT:
            *out_val = (double)expr->int_val;
            return TRUE;

        case EXPR_UNARY: {
            double inner = 0.0;
            if (!comptime_eval_float(ctx, expr->unary.operand, &inner)) return FALSE;
            if (expr->unary.op == TOK_MINUS) {
                *out_val = -inner;
                return TRUE;
            }
            return FALSE;
        }

        case EXPR_BINARY: {
            double left = 0.0, right = 0.0;
            if (!comptime_eval_float(ctx, expr->binary.left, &left)) return FALSE;
            if (!comptime_eval_float(ctx, expr->binary.right, &right)) return FALSE;

            switch (expr->binary.op) {
                case TOK_PLUS:  *out_val = left + right; return TRUE;
                case TOK_MINUS: *out_val = left - right; return TRUE;
                case TOK_STAR:  *out_val = left * right; return TRUE;
                case TOK_SLASH:
                    if (right == 0.0) return FALSE;
                    *out_val = left / right;
                    return TRUE;
                default: return FALSE;
            }
        }

        case EXPR_BLOCK:
            if (expr->block.result_expr) {
                return comptime_eval_float(ctx, expr->block.result_expr, out_val);
            }
            if (expr->block.stmt_count > 0) {
                AstStmt *last_stmt = expr->block.stmts[expr->block.stmt_count - 1];
                if (last_stmt && last_stmt->kind == STMT_EXPR) {
                    return comptime_eval_float(ctx, last_stmt->expr, out_val);
                }
            }
            return FALSE;

        case EXPR_COMPTIME:
            return comptime_eval_float(ctx, expr->comptime_expr.expr, out_val);

        default:
            return FALSE;
    }
}

AstExpr *comptime_fold_expr(ComptimeContext *ctx, AstExpr *expr) {
    if (!expr) return NULL;

    i64 int_res = 0;
    if (comptime_eval_int(ctx, expr, &int_res)) {
        return ast_make_int(ctx->arena, int_res, expr->line, expr->col);
    }

    double float_res = 0.0;
    if (expr->kind != EXPR_INT && comptime_eval_float(ctx, expr, &float_res)) {
        return ast_make_float(ctx->arena, float_res, expr->line, expr->col);
    }

    return expr;
}
