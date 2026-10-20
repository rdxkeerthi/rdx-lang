/*
 * ============================================================================
 *  gen_ref.h  —  RDX Generational Reference Safety System
 * ============================================================================
 *  Provides safe object handles without tracing garbage collection.
 *  Handles detect dangling pointers and use-after-free in O(1) time.
 * ============================================================================
 */

#ifndef RDX_GEN_REF_H
#define RDX_GEN_REF_H

#include "core.h"
#include "arena.h"

typedef struct {
    u32 slot_index;
    u32 generation;
} GenRef;

#define GENREF_NULL ((GenRef){ 0xFFFFFFFFU, 0 })

typedef struct {
    void *ptr;
    u32   generation;
    u32   type_tag;
    bool  is_alive;
} GenSlot;

typedef struct {
    GenSlot *slots;
    u32     *free_list;
    u32      free_head;
    u32      capacity;
    u32      count;
    Arena   *arena;
} GenTable;

void    gen_table_init(GenTable *table, Arena *arena, u32 initial_capacity);
GenRef  gen_table_insert(GenTable *table, void *ptr, u32 type_tag);
void   *gen_table_get(const GenTable *table, GenRef ref, u32 expected_type_tag);
bool    gen_table_is_valid(const GenTable *table, GenRef ref);
bool    gen_table_free_slot(GenTable *table, GenRef ref);

#endif /* RDX_GEN_REF_H */
