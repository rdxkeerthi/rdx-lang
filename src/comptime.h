/*
 * ============================================================================
 *  comptime.h  —  RDX Embedded Compile-Time Constant Evaluator
 * ============================================================================
 *  Evaluates clock tree configurations, register offsets, baud rates,
 *  and bitfields at compile-time with ZERO runtime overhead.
 * ============================================================================
 */

#ifndef RDX_COMPTIME_H
#define RDX_COMPTIME_H

#include "core.h"
#include "ast.h"
#include "arena.h"

typedef struct {
    Arena *arena;
    bool   had_error;
} ComptimeContext;

void     comptime_init(ComptimeContext *ctx, Arena *arena);
bool     comptime_eval_int(ComptimeContext *ctx, AstExpr *expr, i64 *out_val);
bool     comptime_eval_float(ComptimeContext *ctx, AstExpr *expr, double *out_val);
AstExpr *comptime_fold_expr(ComptimeContext *ctx, AstExpr *expr);

#endif /* RDX_COMPTIME_H */
