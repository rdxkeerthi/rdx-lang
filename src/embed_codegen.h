/*
 * ============================================================================
 *  embed_codegen.h  —  RDX Freestanding MISRA-C Code Generator
 * ============================================================================
 *  Emits clean, zero-overhead, bare-metal C99 code for:
 *  - ARM Cortex-M (arm-none-eabi-gcc)
 *  - RISC-V (riscv32-unknown-elf-gcc)
 *  - ESP32 / Xtensa
 *  - Kernels & OS drivers
 * ============================================================================
 */

#ifndef RDX_EMBED_CODEGEN_H
#define RDX_EMBED_CODEGEN_H

#include "core.h"
#include "ast.h"
#include "arena.h"

typedef struct {
    char   *buffer;
    usize   count;
    usize   capacity;
    u32     indent_level;
    Arena  *arena;
    bool    had_error;
} EmbedCodeGen;

void        embed_codegen_init(EmbedCodeGen *gen, Arena *arena);
const char *embed_codegen_generate(EmbedCodeGen *gen, AstProgram *prog);

#endif /* RDX_EMBED_CODEGEN_H */
