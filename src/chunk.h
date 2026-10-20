/*
 * ============================================================================
 *  chunk.h  —  RDX Bytecode Chunk Container
 * ============================================================================
 */

#ifndef RDX_CHUNK_H
#define RDX_CHUNK_H

#include "core.h"
#include "arena.h"
#include "opcode.h"
#include "value.h"

typedef struct Chunk {
    u8         *code;
    usize       count;
    usize       capacity;
    ValueArray  constants;
    u32        *lines;
    Arena      *arena;
} Chunk;

void  chunk_init(Chunk *chunk, Arena *arena);
void  chunk_write(Chunk *chunk, u8 byte, u32 line);
u16   chunk_add_constant(Chunk *chunk, RdxValue val);
void  chunk_write_constant(Chunk *chunk, RdxValue val, u32 line);
void  chunk_disassemble(const Chunk *chunk, const char *name);
usize chunk_disassemble_instruction(const Chunk *chunk, usize offset);

#endif /* RDX_CHUNK_H */
