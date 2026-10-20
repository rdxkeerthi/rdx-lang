/*
 * ============================================================================
 *  ir_lower.h  —  RDX AST → RDX-IR Lowering Pass  (v2.0)
 * ============================================================================
 *
 *  Converts the typed AST into RDX-IR SSA form.
 *  Must be run AFTER the type checker and ownership analyzer.
 *
 *  Produces a fully-annotated IrModule with:
 *    - Ownership metadata on every SSA value
 *    - Lifetime regions for every binding
 *    - Resource tracking for file/socket/mutex handles
 *    - φ-node merges at if/loop join points
 *    - Explicit IR_DROP instructions inserted at scope exits
 */

#ifndef RDX_IR_LOWER_H
#define RDX_IR_LOWER_H

#include "core.h"
#include "arena.h"
#include "ast.h"
#include "rdx_ir.h"
#include "type_check.h"

/* ============================================================
 *  Lowering Context
 * ============================================================ */
#define LOWER_MAX_LOCALS   512
#define LOWER_MAX_SCOPES    64
#define LOWER_MAX_BREAKS    64

typedef struct {
    const char *name;
    usize       name_len;
    IrValueId   val_id;      /* SSA value ID for this binding           */
    IrLifetimeId lifetime;
    bool         is_mut;
} LowerLocal;

typedef struct {
    u32          base;       /* first local index in this scope         */
    u32          count;
    IrLifetimeId lifetime;
} LowerScope;

typedef struct {
    IrBlockId break_target;  /* block to jump to on break               */
    IrBlockId cont_target;   /* block to jump to on continue            */
} LowerLoopCtx;

typedef struct {
    IrModule    *module;
    IrFunction  *current_fn;
    IrBlock     *current_block;
    TypeChecker *tc;

    /* Local binding table */
    LowerLocal   locals[LOWER_MAX_LOCALS];
    u32          local_count;

    LowerScope   scopes[LOWER_MAX_SCOPES];
    u32          scope_depth;

    /* Loop context stack */
    LowerLoopCtx loop_stack[LOWER_MAX_SCOPES];
    u32          loop_depth;

    /* Break patch list (jump offsets to patch after loop body) */
    IrBlockId    break_blocks[LOWER_MAX_BREAKS];
    u32          break_count;

    /* Lifetime counter */
    IrLifetimeId next_lifetime;

    /* Error flag */
    bool         had_error;
    Arena       *arena;
} LowerCtx;

/* ============================================================
 *  Public API
 * ============================================================ */

/* Lower an entire RDX program to an IrModule.
 * `tc`   — completed type checker (provides type info).
 * Returns a new IrModule, or NULL on error.                            */
IrModule *ir_lower_program(AstProgram *prog, TypeChecker *tc, Arena *arena);

/* Lower a single function declaration into an existing module.         */
void ir_lower_function(LowerCtx *ctx, AstDecl *fn_decl);

#endif /* RDX_IR_LOWER_H */
