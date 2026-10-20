/*
 * ============================================================================
 *  fiber.c  —  RDX Green Threads (Fibers) Implementation
 * ============================================================================
 */

#include "fiber.h"

void fiber_init(RdxFiber *fiber, u32 id, ObjFunction *fn, u64 capabilities) {
    fiber->fiber_id     = id;
    fiber->state        = FIBER_READY;
    fiber->frame_count  = 0;
    fiber->stack_top    = fiber->stack;
    fiber->capabilities = capabilities;
    fiber->next         = NULL;
    fiber->prev         = NULL;

    /* Initialize task-scoped deterministic arena */
    arena_init(&fiber->task_arena, 16 * 1024);

    if (fn) {
        CallFrame *frame  = &fiber->frames[fiber->frame_count++];
        frame->function   = fn;
        frame->ip         = fn->chunk->code;
        frame->slots      = fiber->stack;
        fiber_push(fiber, val_fn(fn));
    }
}

void fiber_push(RdxFiber *fiber, RdxValue val) {
    if (fiber->stack_top >= fiber->stack + FIBER_STACK_MAX) {
        rdx_print("[ERROR] Fiber stack overflow\n");
        fiber->state = FIBER_ERROR;
        return;
    }
    *fiber->stack_top++ = val;
}

RdxValue fiber_pop(RdxFiber *fiber) {
    if (fiber->stack_top == fiber->stack) {
        rdx_print("[ERROR] Fiber stack underflow\n");
        fiber->state = FIBER_ERROR;
        return val_nil();
    }
    return *(--fiber->stack_top);
}

RdxValue fiber_peek(const RdxFiber *fiber, i32 distance) {
    return fiber->stack_top[-1 - distance];
}

void fiber_free(RdxFiber *fiber) {
    /* Instantly drop entire task arena in O(1) time without GC scanning */
    arena_free(&fiber->task_arena);
    fiber->state = FIBER_COMPLETED;
}
