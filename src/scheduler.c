/*
 * ============================================================================
 *  scheduler.c  —  RDX Fiber Round-Robin Scheduler Implementation
 * ============================================================================
 */

#include "scheduler.h"

void scheduler_init(Scheduler *sched, Arena *arena) {
    sched->head          = NULL;
    sched->current_fiber = NULL;
    sched->fiber_count   = 0;
    sched->next_fiber_id = 1;
    sched->arena         = arena;
}

RdxFiber *scheduler_spawn(Scheduler *sched, ObjFunction *fn, u64 capabilities) {
    RdxFiber *fiber = (RdxFiber*)arena_alloc_zero(sched->arena, sizeof(RdxFiber));
    if (!fiber) return NULL;

    fiber_init(fiber, sched->next_fiber_id++, fn, capabilities);

    /* Insert into circular doubly linked list */
    if (!sched->head) {
        fiber->next = fiber;
        fiber->prev = fiber;
        sched->head = fiber;
    } else {
        RdxFiber *tail = sched->head->prev;
        tail->next  = fiber;
        fiber->prev = tail;
        fiber->next = sched->head;
        sched->head->prev = fiber;
    }

    sched->fiber_count++;
    return fiber;
}

RdxFiber *scheduler_next_ready(Scheduler *sched) {
    if (!sched->head || sched->fiber_count == 0) return NULL;

    RdxFiber *start = sched->current_fiber ? sched->current_fiber->next : sched->head;
    RdxFiber *curr = start;

    do {
        if (curr->state == FIBER_READY || curr->state == FIBER_YIELDED || curr->state == FIBER_RUNNING) {
            curr->state = FIBER_RUNNING;
            sched->current_fiber = curr;
            return curr;
        }
        curr = curr->next;
    } while (curr != start);

    return NULL;
}

void scheduler_yield(Scheduler *sched) {
    if (sched->current_fiber) {
        sched->current_fiber->state = FIBER_YIELDED;
    }
}

void scheduler_finish_current(Scheduler *sched) {
    if (!sched->current_fiber) return;

    RdxFiber *fiber = sched->current_fiber;
    fiber_free(fiber);

    if (sched->fiber_count == 1) {
        sched->head          = NULL;
        sched->current_fiber = NULL;
        sched->fiber_count   = 0;
        return;
    }

    /* Unlink */
    fiber->prev->next = fiber->next;
    fiber->next->prev = fiber->prev;

    if (sched->head == fiber) {
        sched->head = fiber->next;
    }

    sched->current_fiber = fiber->prev;
    sched->fiber_count--;
}

bool scheduler_has_work(const Scheduler *sched) {
    return sched->fiber_count > 0;
}
