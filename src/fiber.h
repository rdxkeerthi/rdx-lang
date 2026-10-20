/*
 * ============================================================================
 *  fiber.h  —  RDX Green Threads (Fibers) & Colorless Concurrency
 * ============================================================================
 *  Fibers enable structured, colorless concurrency inside the VM.
 *  Each fiber owns its lightweight execution stack and dedicated Task Arena.
 *  When a fiber finishes, its Task Arena is dropped in O(1) time.
 * ============================================================================
 */

#ifndef RDX_FIBER_H
#define RDX_FIBER_H

#include "core.h"
#include "arena.h"
#include "value.h"
#include "chunk.h"
#include "capability.h"

#define FIBER_STACK_MAX  2048
#define FIBER_FRAMES_MAX 256

typedef enum {
    FIBER_READY,
    FIBER_RUNNING,
    FIBER_YIELDED,
    FIBER_BLOCKED,
    FIBER_COMPLETED,
    FIBER_ERROR
} FiberState;

typedef struct {
    ObjFunction *function;
    u8          *ip;
    RdxValue    *slots;
} CallFrame;

typedef struct RdxFiber RdxFiber;
struct RdxFiber {
    u32         fiber_id;
    FiberState  state;
    CallFrame   frames[FIBER_FRAMES_MAX];
    u32         frame_count;
    RdxValue    stack[FIBER_STACK_MAX];
    RdxValue   *stack_top;
    u64         capabilities;
    Arena       task_arena;  /* Task-scoped deterministic memory arena */
    RdxFiber   *next;
    RdxFiber   *prev;
};

void      fiber_init(RdxFiber *fiber, u32 id, ObjFunction *fn, u64 capabilities);
void      fiber_push(RdxFiber *fiber, RdxValue val);
RdxValue  fiber_pop(RdxFiber *fiber);
RdxValue  fiber_peek(const RdxFiber *fiber, i32 distance);
void      fiber_free(RdxFiber *fiber);

#endif /* RDX_FIBER_H */
