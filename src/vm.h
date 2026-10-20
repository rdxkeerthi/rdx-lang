/*
 * ============================================================================
 *  vm.h  —  RDX Stack-Based Bytecode Virtual Machine
 * ============================================================================
 */

#ifndef RDX_VM_H
#define RDX_VM_H

#include "core.h"
#include "arena.h"
#include "gen_ref.h"
#include "value.h"
#include "chunk.h"
#include "fiber.h"
#include "scheduler.h"
#include "capability.h"
#include "ffi.h"

#define VM_MAX_GLOBALS 512

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_SECURITY_ERROR
} InterpretResult;

typedef struct {
    const char *name;
    usize       name_len;
    RdxValue    value;
} GlobalVar;

typedef struct {
    Scheduler sched;
    GenTable  gen_table;
    Arena     vm_arena;
    GlobalVar globals[VM_MAX_GLOBALS];
    u32       global_count;
    u64       default_capabilities;
} VM;

void            vm_init(VM *vm, u64 capabilities);
void            vm_free(VM *vm);
bool            vm_get_global(const VM *vm, const char *name, usize length, RdxValue *out);
void            vm_set_global(VM *vm, const char *name, usize length, RdxValue value);
InterpretResult vm_run_function(VM *vm, ObjFunction *fn);
InterpretResult vm_interpret_source(VM *vm, const char *source, usize length, const char *filename);

#endif /* RDX_VM_H */
