/*
 * ============================================================================
 *  ir_opt.h  —  RDX-IR Optimization Passes  (v2.0)
 * ============================================================================
 *
 *  Optimization pipeline over IrModule / IrFunction.
 *  Passes run in order:
 *
 *    1. Constant Folding & Propagation  — fold IR_ADD(const, const) → const
 *    2. Dead Code Elimination           — remove unreachable blocks & unused defs
 *    3. Copy Propagation               — eliminate trivial IR_MOVE chains
 *    4. Common Subexpression Elim.     — deduplicate identical computations
 *    5. Inlining                       — expand small functions at call sites
 *    6. Strength Reduction             — replace mul/div with shifts where safe
 */

#ifndef RDX_IR_OPT_H
#define RDX_IR_OPT_H

#include "rdx_ir.h"

/* ============================================================
 *  Optimization Level
 * ============================================================ */
typedef enum {
    OPT_LEVEL_NONE  = 0,   /* -O0: no optimization; debug-friendly      */
    OPT_LEVEL_BASIC = 1,   /* -O1: constant folding + DCE               */
    OPT_LEVEL_FULL  = 2,   /* -O2: all passes (default production)      */
    OPT_LEVEL_SIZE  = 3,   /* -Os: optimize for binary size             */
} OptLevel;

/* ============================================================
 *  Optimization Statistics (returned after a run)
 * ============================================================ */
typedef struct {
    u32 constants_folded;
    u32 dead_instrs_removed;
    u32 dead_blocks_removed;
    u32 copies_propagated;
    u32 cse_hits;
    u32 functions_inlined;
    u32 strength_reductions;
} OptStats;

/* ============================================================
 *  Public API
 * ============================================================ */

/* Run all applicable passes at the given level.
 * Modifies the module IN PLACE.
 * Returns aggregated stats.                                           */
OptStats ir_optimize(IrModule *mod, OptLevel level);

/* Individual passes — can be invoked independently                    */
OptStats ir_pass_const_fold(IrFunction *fn);
OptStats ir_pass_dce(IrFunction *fn);
OptStats ir_pass_copy_prop(IrFunction *fn);
OptStats ir_pass_cse(IrFunction *fn);
OptStats ir_pass_strength_reduce(IrFunction *fn);
OptStats ir_pass_inline(IrModule *mod, u32 max_inline_instrs);

/* Print optimization statistics to stdout                            */
void ir_opt_print_stats(const OptStats *stats);

#endif /* RDX_IR_OPT_H */
