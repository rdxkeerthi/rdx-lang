/* codegen.h — RDX JIT Engine (Steps 1-4: functions, structs, arrays, arenas) */
#ifndef RDX_CODEGEN_H
#define RDX_CODEGEN_H
#include "parser.h"
#include "diag.h"

#define JIT_MAX_VARS      128   /* global variable slots          */
#define JIT_MAX_LOCALS     64   /* local variable slots per frame */
#define JIT_MAX_FRAMES     32   /* max call-stack depth           */
#define JIT_MAX_FNS        64   /* max user-defined functions     */
#define JIT_MAX_SHAPES     32   /* max shape (struct) types       */
#define JIT_MAX_FIELDS     16   /* max fields per shape           */
#define JIT_MAX_INSTANCES  64   /* max live struct instances      */
#define JIT_MAX_ARRAYS     128  /* max live array objects         */
#define JIT_MAX_ELEMS      256  /* max elements per array         */
#define JIT_MAX_ARENAS      8   /* max live arenas                */
#define ARENA_MAX_SIZE  (1024*1024) /* 1 MB max per arena         */

/* ── One variable / value slot ────────────────────────────────── */
typedef struct {
    char   name[64];
    char   type[64];   /* "num" "chr" "flt" "blo" "dob" "struct" "array" */
    i64    ival;
    double dval;
    char   sval[256];
    /* Step 2: struct instance reference */
    int    inst_id;    /* index into jit->instances[], -1 if not struct */
    /* Step 3: array reference */
    int    arr_id;     /* index into jit->arrays[], -1 if not array     */
    /* Step 5: result/option flags */
    int    is_ok;      /* 1 = Ok variant    */
    int    is_err;     /* 1 = Err variant   */
    int    is_some;    /* 1 = Some variant  */
    int    is_none;    /* 1 = None variant  */
} JitVar;

/* ── Struct (shape) type definition ───────────────────────────── */
typedef struct {
    char shape_name[64];
    char field_names[JIT_MAX_FIELDS][64];
    char field_types[JIT_MAX_FIELDS][64];
    int  field_count;
} JitShape;

/* ── One live struct instance ─────────────────────────────────── */
typedef struct {
    char   shape_name[64];
    JitVar fields[JIT_MAX_FIELDS];
    int    field_count;
    int    in_use;
} JitInstance;

/* ── One live array object ────────────────────────────────────── */
typedef struct {
    JitVar elems[JIT_MAX_ELEMS];
    int    len;
    int    in_use;
} JitArray;

/* ── Arena allocator (Step 4) ─────────────────────────────────── */
typedef struct {
    char   name[64];
    void  *mem;      /* base pointer from VirtualAlloc         */
    usize  capacity; /* total bytes                            */
    usize  used;     /* bump-pointer offset                    */
    int    in_use;
} JitArena;

/* ── One activation frame pushed per function call ────────────── */
typedef struct {
    JitVar locals[JIT_MAX_LOCALS];
    int    local_count;
} JitFrame;

/* ── Entry in the user-defined function registry ──────────────── */
typedef struct {
    char name[64];
    int  node_idx;   /* index into rdx_nodes[] for the NODE_FN_DECL */
} JitFn;

#define JIT_MAX_EXTERN_FNS 32

/* ── Entry in the external function registry ──────────────────── */
typedef struct {
    char name[64];
    char lib_name[128];
    void *fn_ptr;     /* resolved function pointer */
    int  node_idx;    /* index into rdx_nodes[] for signature info */
} JitExternFn;

/* ── JIT Engine ───────────────────────────────────────────────── */
typedef struct {
    /* Global (top-level) variable table */
    JitVar vars[JIT_MAX_VARS];
    int    var_count;

    /* User-defined function registry (populated in Pass 1 of jit_run) */
    JitFn  fns[JIT_MAX_FNS];
    int    fn_count;

    /* Shape (struct type) registry */
    JitShape shapes[JIT_MAX_SHAPES];
    int      shape_count;

    /* Live struct instances */
    JitInstance instances[JIT_MAX_INSTANCES];
    int         instance_count;

    /* Live arrays */
    JitArray arrays[JIT_MAX_ARRAYS];
    int      array_count;

    /* Arena allocators */
    JitArena arenas[JIT_MAX_ARENAS];
    int      arena_count;

    /* Call stack — frame_depth==0 means top-level */
    JitFrame call_stack[JIT_MAX_FRAMES];
    int      frame_depth;

    /* External function registry */
    JitExternFn extern_fns[JIT_MAX_EXTERN_FNS];
    int         extern_fn_count;
    
    /* Dynamically loaded modules */
    void*       modules[16];
    int         module_count;

    /* Return-value slot + unwind flag */
    JitVar   ret_val;
    int      return_flag;   /* 1 = ret was executed; unwind blocks/loops */

    /* Other control-flow flags */
    int    had_error;
    int    continue_flag;
    int    break_flag;
} JitEngine;

int  jit_run (JitEngine *jit, int root_node);
void jit_init(JitEngine *jit);

#endif
