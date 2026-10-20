/*
 * ============================================================================
 *  chunk.c  —  RDX Bytecode Chunk Implementation & Disassembler
 * ============================================================================
 */

#include "chunk.h"

const char *opcode_name(OpCode op) {
    switch (op) {
        case OP_CONSTANT:      return "OP_CONSTANT";
        case OP_NIL:           return "OP_NIL";
        case OP_TRUE:          return "OP_TRUE";
        case OP_FALSE:         return "OP_FALSE";
        case OP_POP:           return "OP_POP";
        case OP_DUP:           return "OP_DUP";
        case OP_GET_LOCAL:     return "OP_GET_LOCAL";
        case OP_SET_LOCAL:     return "OP_SET_LOCAL";
        case OP_GET_GLOBAL:    return "OP_GET_GLOBAL";
        case OP_SET_GLOBAL:    return "OP_SET_GLOBAL";
        case OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL";
        case OP_ADD:           return "OP_ADD";
        case OP_SUB:           return "OP_SUB";
        case OP_MUL:           return "OP_MUL";
        case OP_DIV:           return "OP_DIV";
        case OP_MOD:           return "OP_MOD";
        case OP_NEG:           return "OP_NEG";
        case OP_NOT:           return "OP_NOT";
        case OP_BIT_NOT:       return "OP_BIT_NOT";
        case OP_BIT_AND:       return "OP_BIT_AND";
        case OP_BIT_OR:        return "OP_BIT_OR";
        case OP_BIT_XOR:       return "OP_BIT_XOR";
        case OP_SHL:           return "OP_SHL";
        case OP_SHR:           return "OP_SHR";
        case OP_EQ:            return "OP_EQ";
        case OP_NEQ:           return "OP_NEQ";
        case OP_LT:            return "OP_LT";
        case OP_LTE:           return "OP_LTE";
        case OP_GT:            return "OP_GT";
        case OP_GTE:           return "OP_GTE";
        case OP_JUMP:          return "OP_JUMP";
        case OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
        case OP_LOOP:          return "OP_LOOP";
        case OP_CALL:          return "OP_CALL";
        case OP_RETURN:        return "OP_RETURN";
        case OP_PRINT:         return "OP_PRINT";
        case OP_SPAWN:         return "OP_SPAWN";
        case OP_YIELD:         return "OP_YIELD";
        case OP_CAP_REQ:       return "OP_CAP_REQ";
        case OP_CAP_INVOKE:    return "OP_CAP_INVOKE";
        case OP_HALT:          return "OP_HALT";
        default:               return "<UNKNOWN_OP>";
    }
}

void chunk_init(Chunk *chunk, Arena *arena) {
    chunk->arena    = arena;
    chunk->count    = 0;
    chunk->capacity = 64;
    chunk->code     = (u8*)arena_alloc(arena, chunk->capacity);
    chunk->lines    = (u32*)arena_alloc(arena, sizeof(u32) * chunk->capacity);
    value_array_init(&chunk->constants, arena);
}

void chunk_write(Chunk *chunk, u8 byte, u32 line) {
    if (chunk->count >= chunk->capacity) {
        usize new_cap = chunk->capacity * 2;
        u8   *new_code  = (u8*)arena_alloc(chunk->arena, new_cap);
        u32  *new_lines = (u32*)arena_alloc(chunk->arena, sizeof(u32) * new_cap);

        my_memcpy(new_code, chunk->code, chunk->capacity);
        my_memcpy(new_lines, chunk->lines, sizeof(u32) * chunk->capacity);

        chunk->code     = new_code;
        chunk->lines    = new_lines;
        chunk->capacity = new_cap;
    }
    chunk->code[chunk->count]  = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

u16 chunk_add_constant(Chunk *chunk, RdxValue val) {
    /* Check for deduplication for ints and strings */
    for (u32 i = 0; i < chunk->constants.count; i++) {
        if (value_equals(chunk->constants.values[i], val)) {
            return (u16)i;
        }
    }
    value_array_write(&chunk->constants, val);
    return (u16)(chunk->constants.count - 1);
}

void chunk_write_constant(Chunk *chunk, RdxValue val, u32 line) {
    u16 const_idx = chunk_add_constant(chunk, val);
    chunk_write(chunk, (u8)OP_CONSTANT, line);
    chunk_write(chunk, (u8)(const_idx & 0xFF), line);
    chunk_write(chunk, (u8)((const_idx >> 8) & 0xFF), line);
}

static usize simple_instruction(const char *name, usize offset) {
    rdx_print(name);
    rdx_print("\n");
    return offset + 1;
}

static usize byte_instruction(const char *name, const Chunk *chunk, usize offset) {
    u8 slot = chunk->code[offset + 1];
    rdx_print(name);
    rdx_print(" ");
    rdx_print_num((i64)slot);
    rdx_print("\n");
    return offset + 2;
}

static usize constant_instruction(const char *name, const Chunk *chunk, usize offset) {
    u16 const_idx = (u16)(chunk->code[offset + 1] | (chunk->code[offset + 2] << 8));
    rdx_print(name);
    rdx_print(" #");
    rdx_print_num((i64)const_idx);
    rdx_print(" (");
    if (const_idx < chunk->constants.count) {
        value_print(chunk->constants.values[const_idx]);
    }
    rdx_print(")\n");
    return offset + 3;
}

static usize jump_instruction(const char *name, i32 sign, const Chunk *chunk, usize offset) {
    u16 jump = (u16)(chunk->code[offset + 1] | (chunk->code[offset + 2] << 8));
    rdx_print(name);
    rdx_print(" -> ");
    rdx_print_num((i64)(offset + 3 + sign * jump));
    rdx_print("\n");
    return offset + 3;
}

usize chunk_disassemble_instruction(const Chunk *chunk, usize offset) {
    rdx_print_num((i64)offset);
    rdx_print("  ");

    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        rdx_print("   | ");
    } else {
        rdx_print_num((i64)chunk->lines[offset]);
        rdx_print(" ");
    }

    u8 instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simple_instruction("OP_NIL", offset);
        case OP_TRUE:
            return simple_instruction("OP_TRUE", offset);
        case OP_FALSE:
            return simple_instruction("OP_FALSE", offset);
        case OP_POP:
            return simple_instruction("OP_POP", offset);
        case OP_DUP:
            return simple_instruction("OP_DUP", offset);
        case OP_GET_LOCAL:
            return constant_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return constant_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL:
            return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_ADD:
            return simple_instruction("OP_ADD", offset);
        case OP_SUB:
            return simple_instruction("OP_SUB", offset);
        case OP_MUL:
            return simple_instruction("OP_MUL", offset);
        case OP_DIV:
            return simple_instruction("OP_DIV", offset);
        case OP_MOD:
            return simple_instruction("OP_MOD", offset);
        case OP_NEG:
            return simple_instruction("OP_NEG", offset);
        case OP_NOT:
            return simple_instruction("OP_NOT", offset);
        case OP_BIT_NOT:
            return simple_instruction("OP_BIT_NOT", offset);
        case OP_BIT_AND:
            return simple_instruction("OP_BIT_AND", offset);
        case OP_BIT_OR:
            return simple_instruction("OP_BIT_OR", offset);
        case OP_BIT_XOR:
            return simple_instruction("OP_BIT_XOR", offset);
        case OP_SHL:
            return simple_instruction("OP_SHL", offset);
        case OP_SHR:
            return simple_instruction("OP_SHR", offset);
        case OP_EQ:
            return simple_instruction("OP_EQ", offset);
        case OP_NEQ:
            return simple_instruction("OP_NEQ", offset);
        case OP_LT:
            return simple_instruction("OP_LT", offset);
        case OP_LTE:
            return simple_instruction("OP_LTE", offset);
        case OP_GT:
            return simple_instruction("OP_GT", offset);
        case OP_GTE:
            return simple_instruction("OP_GTE", offset);
        case OP_JUMP:
            return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byte_instruction("OP_CALL", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        case OP_PRINT:
            return byte_instruction("OP_PRINT", chunk, offset);
        case OP_SPAWN:
            return byte_instruction("OP_SPAWN", chunk, offset);
        case OP_YIELD:
            return simple_instruction("OP_YIELD", offset);
        case OP_CAP_REQ:
            return constant_instruction("OP_CAP_REQ", chunk, offset);
        case OP_CAP_INVOKE:
            return byte_instruction("OP_CAP_INVOKE", chunk, offset);
        case OP_HALT:
            return simple_instruction("OP_HALT", offset);
        default:
            rdx_print("Unknown opcode ");
            rdx_print_num((i64)instruction);
            rdx_print("\n");
            return offset + 1;
    }
}

void chunk_disassemble(const Chunk *chunk, const char *name) {
    rdx_print("== ");
    rdx_print(name);
    rdx_print(" ==\n");

    for (usize offset = 0; offset < chunk->count;) {
        offset = chunk_disassemble_instruction(chunk, offset);
    }
}
