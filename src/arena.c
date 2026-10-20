/*
 * ============================================================================
 *  arena.c  —  RDX Deterministic Arena Allocator Implementation
 * ============================================================================
 */

#include "arena.h"

static inline usize align_forward(usize ptr, usize align) {
    usize rem = ptr % align;
    if (rem == 0) return ptr;
    return ptr + (align - rem);
}

static ArenaBlock *arena_new_block(usize capacity) {
    usize header_size = align_forward(sizeof(ArenaBlock), ARENA_ALIGNMENT);
    usize total_size  = header_size + capacity;
    
    /* Allocate virtual memory page via platform core rdx_alloc */
    u8 *raw = (u8*)rdx_alloc(total_size);
    if (!raw) return NULL;
    
    ArenaBlock *block = (ArenaBlock*)raw;
    block->next       = NULL;
    block->capacity   = capacity;
    block->used       = 0;
    block->memory     = raw + header_size;
    return block;
}

void arena_init(Arena *arena, usize default_block_size) {
    if (!arena) return;
    if (default_block_size == 0) {
        default_block_size = ARENA_DEFAULT_BLOCK_SIZE;
    }
    arena->default_block_size = default_block_size;
    arena->first_block        = arena_new_block(default_block_size);
    arena->current_block      = arena->first_block;
    arena->total_allocated    = default_block_size;
}

Arena arena_create(usize default_block_size) {
    Arena arena;
    arena_init(&arena, default_block_size);
    return arena;
}

void *arena_alloc_aligned(Arena *arena, usize size, usize alignment) {
    if (!arena || size == 0) return NULL;
    if (alignment == 0) alignment = ARENA_ALIGNMENT;

    if (!arena->current_block) {
        arena_init(arena, arena->default_block_size);
        if (!arena->current_block) return NULL;
    }

    ArenaBlock *block = arena->current_block;
    usize current_ptr = (usize)(block->memory + block->used);
    usize aligned_ptr = align_forward(current_ptr, alignment);
    usize offset      = aligned_ptr - (usize)block->memory;

    if (offset + size <= block->capacity) {
        block->used = offset + size;
        return (void*)aligned_ptr;
    }

    /* Current block cannot accommodate; allocate a new block */
    usize next_cap = arena->default_block_size;
    if (size + alignment > next_cap) {
        next_cap = size + alignment + ARENA_DEFAULT_BLOCK_SIZE;
    }

    ArenaBlock *new_blk = arena_new_block(next_cap);
    if (!new_blk) return NULL;

    block->next          = new_blk;
    arena->current_block = new_blk;
    arena->total_allocated += next_cap;

    usize new_ptr = align_forward((usize)new_blk->memory, alignment);
    usize new_off = new_ptr - (usize)new_blk->memory;
    new_blk->used = new_off + size;
    return (void*)new_ptr;
}

void *arena_alloc(Arena *arena, usize size) {
    return arena_alloc_aligned(arena, size, ARENA_ALIGNMENT);
}

void *arena_alloc_zero(Arena *arena, usize size) {
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        my_memset(ptr, 0, size);
    }
    return ptr;
}

char *arena_strdup(Arena *arena, const char *str) {
    if (!str) return NULL;
    usize len = my_strlen(str);
    char *copy = (char*)arena_alloc(arena, len + 1);
    if (copy) {
        my_memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

char *arena_strndup(Arena *arena, const char *str, usize length) {
    if (!str) return NULL;
    char *copy = (char*)arena_alloc(arena, length + 1);
    if (copy) {
        my_memcpy(copy, str, length);
        copy[length] = '\0';
    }
    return copy;
}

void arena_reset(Arena *arena) {
    if (!arena) return;
    ArenaBlock *curr = arena->first_block;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
    arena->current_block = arena->first_block;
}

void arena_free(Arena *arena) {
    if (!arena) return;
    ArenaBlock *curr = arena->first_block;
    while (curr) {
        ArenaBlock *next = curr->next;
        rdx_va_free((void*)curr);
        curr = next;
    }
    arena->first_block   = NULL;
    arena->current_block = NULL;
    arena->total_allocated = 0;
}

ArenaMarker arena_save(Arena *arena) {
    ArenaMarker marker;
    if (arena && arena->current_block) {
        marker.block = arena->current_block;
        marker.used  = arena->current_block->used;
    } else {
        marker.block = NULL;
        marker.used  = 0;
    }
    return marker;
}

void arena_restore(Arena *arena, ArenaMarker marker) {
    if (!arena || !marker.block) return;
    arena->current_block = marker.block;
    arena->current_block->used = marker.used;

    /* Reset subsequent blocks */
    ArenaBlock *curr = marker.block->next;
    while (curr) {
        curr->used = 0;
        curr = curr->next;
    }
}
