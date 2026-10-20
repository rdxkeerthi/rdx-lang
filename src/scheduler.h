/*
 * ============================================================================
 *  scheduler.h  —  RDX Colorless Concurrency Round-Robin Scheduler
 * ============================================================================
 */

#ifndef RDX_SCHEDULER_H
#define RDX_SCHEDULER_H

#include "core.h"
#include "fiber.h"
#include "arena.h"

typedef struct {
    RdxFiber *head;
    RdxFiber *current_fiber;
    u32       fiber_count;
    u32       next_fiber_id;
    Arena    *arena;
} Scheduler;

void      scheduler_init(Scheduler *sched, Arena *arena);
RdxFiber *scheduler_spawn(Scheduler *sched, ObjFunction *fn, u64 capabilities);
RdxFiber *scheduler_next_ready(Scheduler *sched);
void      scheduler_yield(Scheduler *sched);
void      scheduler_finish_current(Scheduler *sched);
bool      scheduler_has_work(const Scheduler *sched);

#endif /* RDX_SCHEDULER_H */
