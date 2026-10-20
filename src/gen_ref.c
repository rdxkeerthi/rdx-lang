/*
 * ============================================================================
 *  gen_ref.c  —  RDX Generational Reference Implementation
 * ============================================================================
 */

#include "gen_ref.h"

#define GEN_TABLE_DEFAULT_CAP 1024

void gen_table_init(GenTable *table, Arena *arena, u32 initial_capacity) {
    if (!table || !arena) return;
    if (initial_capacity == 0) initial_capacity = GEN_TABLE_DEFAULT_CAP;

    table->arena     = arena;
    table->capacity  = initial_capacity;
    table->count     = 0;
    table->free_head = 0;

    table->slots     = (GenSlot*)arena_alloc_zero(arena, sizeof(GenSlot) * table->capacity);
    table->free_list = (u32*)arena_alloc(arena, sizeof(u32) * table->capacity);

    for (u32 i = 0; i < table->capacity; i++) {
        table->free_list[i] = i;
        table->slots[i].generation = 1;
        table->slots[i].is_alive   = FALSE;
        table->slots[i].ptr        = NULL;
    }
}

GenRef gen_table_insert(GenTable *table, void *ptr, u32 type_tag) {
    if (!table || !table->slots) return GENREF_NULL;

    if (table->free_head >= table->capacity) {
        /* Expand table inside arena */
        u32 old_cap = table->capacity;
        u32 new_cap = old_cap * 2;
        
        GenSlot *new_slots = (GenSlot*)arena_alloc_zero(table->arena, sizeof(GenSlot) * new_cap);
        u32     *new_free  = (u32*)arena_alloc(table->arena, sizeof(u32) * new_cap);

        my_memcpy(new_slots, table->slots, sizeof(GenSlot) * old_cap);
        my_memcpy(new_free, table->free_list, sizeof(u32) * old_cap);

        for (u32 i = old_cap; i < new_cap; i++) {
            new_free[i] = i;
            new_slots[i].generation = 1;
            new_slots[i].is_alive   = FALSE;
            new_slots[i].ptr        = NULL;
        }

        table->slots     = new_slots;
        table->free_list = new_free;
        table->capacity  = new_cap;
    }

    u32 slot_idx = table->free_list[table->free_head++];
    GenSlot *slot = &table->slots[slot_idx];

    slot->ptr      = ptr;
    slot->type_tag = type_tag;
    slot->is_alive = TRUE;
    table->count++;

    GenRef ref;
    ref.slot_index = slot_idx;
    ref.generation = slot->generation;
    return ref;
}

void *gen_table_get(const GenTable *table, GenRef ref, u32 expected_type_tag) {
    if (!table || ref.slot_index >= table->capacity) return NULL;

    const GenSlot *slot = &table->slots[ref.slot_index];
    if (!slot->is_alive || slot->generation != ref.generation) {
        return NULL; /* Expired or stale reference (preventing use-after-free) */
    }

    if (expected_type_tag != 0 && slot->type_tag != expected_type_tag) {
        return NULL; /* Type mismatch */
    }

    return slot->ptr;
}

bool gen_table_is_valid(const GenTable *table, GenRef ref) {
    if (!table || ref.slot_index >= table->capacity) return FALSE;
    const GenSlot *slot = &table->slots[ref.slot_index];
    return slot->is_alive && (slot->generation == ref.generation);
}

bool gen_table_free_slot(GenTable *table, GenRef ref) {
    if (!table || ref.slot_index >= table->capacity) return FALSE;

    GenSlot *slot = &table->slots[ref.slot_index];
    if (!slot->is_alive || slot->generation != ref.generation) {
        return FALSE;
    }

    slot->is_alive = FALSE;
    slot->generation++; /* Increment generation to invalidate all existing GenRefs */
    slot->ptr = NULL;

    table->free_list[--table->free_head] = ref.slot_index;
    table->count--;
    return TRUE;
}
