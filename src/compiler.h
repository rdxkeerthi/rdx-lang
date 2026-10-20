/*
 * ============================================================================
 *  compiler.h  —  RDX AST-to-Bytecode Compiler
 * ============================================================================
 */

#ifndef RDX_COMPILER_H
#define RDX_COMPILER_H

#include "core.h"
#include "ast.h"
#include "chunk.h"
#include "value.h"

#define MAX_LOCALS 256
#define MAX_BREAKS 64

typedef struct {
    const char *name;
    usize       length;
    i32         depth;
    bool        is_mut;
} Local;

typedef struct LoopContext LoopContext;
struct LoopContext {
    usize        loop_start;
    usize        break_jumps[MAX_BREAKS];
    u32          break_count;
    i32          scope_depth;
    LoopContext *enclosing;
};

typedef struct Compiler Compiler;
struct Compiler {
    Compiler     *enclosing;
    ObjFunction  *function;
    Chunk        *chunk;
    Local         locals[MAX_LOCALS];
    u32           local_count;
    i32           scope_depth;
    LoopContext  *current_loop;
    Arena        *arena;
    bool          had_error;
};

void         compiler_init(Compiler *compiler, Compiler *enclosing, ObjFunction *function, Chunk *chunk, Arena *arena);
ObjFunction *compiler_compile_program(AstProgram *program, Arena *arena);

#endif /* RDX_COMPILER_H */
