/*
 * ============================================================================
 *  arena.h  —  RDX Deterministic Arena Allocator
 * ============================================================================
 *  Core Architectural Pillar:
 *  - Fast contiguous bump allocation.
 *  - Zero GC pauses, zero reference counting overhead.
 *  - Block-chained growth using OS virtual memory pages.
 *  - Instant bulk resets and region markers for scope/task lifecycles.
 * ============================================================================
 */

#ifndef RDX_ARENA_H
#define RDX_ARENA_H

#include "core.h"

#define ARENA_DEFAULT_BLOCK_SIZE (64 * 1024) /* 64 KiB default block */
#define ARENA_ALIGNMENT 8

typedef struct ArenaBlock ArenaBlock;

struct ArenaBlock {
    ArenaBlock *next;
    usize       capacity;
    usize       used;
    u8         *memory;
};

typedef struct {
    ArenaBlock *first_block;
    ArenaBlock *current_block;
    usize       default_block_size;
    usize       total_allocated;
} Arena;

/* Marker for saving and restoring arena allocation state */
typedef struct {
    ArenaBlock *block;
    usize       used;
} ArenaMarker;

/* Lifecycle */
void  arena_init(Arena *arena, usize default_block_size);
Arena arena_create(usize default_block_size);
void  arena_reset(Arena *arena);
void  arena_free(Arena *arena);

/* Allocation */
void *arena_alloc(Arena *arena, usize size);
void *arena_alloc_aligned(Arena *arena, usize size, usize alignment);
void *arena_alloc_zero(Arena *arena, usize size);

/* String Helpers */
char *arena_strdup(Arena *arena, const char *str);
char *arena_strndup(Arena *arena, const char *str, usize length);

/* Region Save / Restore */
ArenaMarker arena_save(Arena *arena);
void        arena_restore(Arena *arena, ArenaMarker marker);

#endif /* RDX_ARENA_H */
