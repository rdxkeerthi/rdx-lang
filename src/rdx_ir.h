/*
 * ============================================================================
 *  rdx_ir.h  —  RDX Intermediate Representation (RDX-IR)
 * ============================================================================
 *
 *  PURPOSE
 *  -------
 *  This IR sits between the AST and the LLVM IR backend. Unlike LLVM IR,
 *  RDX-IR is a HIGH-LEVEL IR that deliberately preserves:
 *
 *    1. Ownership metadata     — who owns a value and when it is dropped
 *    2. Lifetime regions       — the lexical scope a value is alive within
 *    3. Resource handles       — file/socket/mutex lifecycle guarantees
 *    4. Concurrency effects    — which tasks share which memory regions
 *    5. Borrow information     — shared (&) vs exclusive (&mut) references
 *
 *  This allows the Optimizer and the LLVM backend to emit correct, safe,
 *  zero-UB machine code without re-analyzing the source semantics.
 *
 *  IR FORM
 *  -------
 *  RDX-IR is a Static Single Assignment (SSA) graph.
 *  Every value is produced exactly once (by an IrInstr).
 *  Values are identified by a u32 "value ID" (IrValueId).
 *  Control flow is expressed as a graph of IrBlock nodes.
 *
 *  PIPELINE POSITION
 *  -----------------
 *    AST  →  [Semantic / Type / Lifetime / Resource Checker]
 *         →  RDX-IR   ← YOU ARE HERE
 *         →  [RDX-IR Optimizer]
 *         →  LLVM IR  (or direct JIT via rdx_jit engine)
 *
 * ============================================================================
 */

#ifndef RDX_IR_H
#define RDX_IR_H

#include "core.h"   /* u8, u16, u32, u64, i32, i64, bool, NULL */

/* ============================================================
 *  §1  Forward Declarations
 * ============================================================ */

typedef struct IrType        IrType;
typedef struct IrValue       IrValue;
typedef struct IrInstr       IrInstr;
typedef struct IrBlock       IrBlock;
typedef struct IrFunction    IrFunction;
typedef struct IrModule      IrModule;
typedef struct IrLifetime    IrLifetime;
typedef struct IrOwnership   IrOwnership;
typedef struct IrResource    IrResource;
typedef struct IrEffect      IrEffect;

/* Numeric IDs */
typedef u32 IrValueId;
typedef u32 IrBlockId;
typedef u32 IrTypeId;
typedef u32 IrLifetimeId;
typedef u32 IrResourceId;
#define IR_ID_INVALID 0xFFFFFFFFU

/* ============================================================
 *  §2  Type System
 * ============================================================ */

typedef enum {
    /* Scalar primitives */
    IR_TY_VOID,
    IR_TY_BOOL,          /* blo  — 1 byte                        */
    IR_TY_BYTE,          /* byt  — u8                            */
    IR_TY_INT64,         /* num  — i64                           */
    IR_TY_FLOAT32,       /* flt  — f32                           */
    IR_TY_FLOAT64,       /* dob  — f64                           */

    /* Compound types */
    IR_TY_PTR,           /* raw unsafe pointer (ptr)             */
    IR_TY_REF,           /* &T   — immutable borrow              */
    IR_TY_REF_MUT,       /* &mut T — exclusive (mutable) borrow  */
    IR_TY_SLICE,         /* []T  — fat pointer (ptr + len)       */
    IR_TY_STR,           /* chr  — UTF-8 string slice            */
    IR_TY_TUPLE,         /* (T0, T1, ...)                        */
    IR_TY_STRUCT,
    IR_TY_ENUM,
    IR_TY_FUNCTION,      /* fn(A,B,...) -> R                     */
    IR_TY_CLOSURE,
    IR_TY_RESULT,        /* Result[T]                            */
    IR_TY_OPTION,        /* Option[T]                            */
    IR_TY_RESOURCE,      /* tracked OS resource handle           */

    IR_TY_COUNT,
} IrTypeKind;

/*
 * IrType — full type descriptor.
 *
 * For compound types (struct, slice, fn, …) `inner` points to an array
 * of sub-type IDs; `inner_count` gives its length.
 *
 * `align_bytes` and `size_bytes` are populated by the type-layout pass
 * once all struct members are known.  For opaque/forward-declared types
 * both remain 0 until finalized.
 */
struct IrType {
    IrTypeKind  kind;
    u32         size_bytes;     /* 0 = unknown / opaque             */
    u32         align_bytes;    /* 0 = unknown                      */
    IrTypeId   *inner;          /* sub-types (fields, params, …)    */
    u32         inner_count;
    const char *name;           /* human-readable debug name        */
    bool        is_copy;        /* true → value is implicitly copied (scalars) */
    bool        is_send;        /* true → safe to send across task boundaries   */
    bool        is_sync;        /* true → safe to share (& access) across tasks */
};

/* ============================================================
 *  §3  Lifetimes
 *
 *  A lifetime is a compiler-assigned "live region" for a value.
 *  Lifetimes form a tree: child lifetimes nest inside their parent.
 *  The Borrow Checker ensures no reference outlives its referent's lifetime.
 * ============================================================ */

struct IrLifetime {
    IrLifetimeId  id;
    IrLifetimeId  parent;       /* IR_ID_INVALID for the function root lifetime  */
    u32           depth;        /* nesting depth (0 = function scope)            */
    IrBlockId     entry_block;  /* IR block where this lifetime begins           */
    IrBlockId     exit_block;   /* IR block where this lifetime ends             */
    const char   *debug_name;   /* e.g. "'a", "'file_scope"                     */
};

/* ============================================================
 *  §4  Ownership State
 *
 *  Tracks the move/borrow/drop lifecycle of every non-Copy value.
 *  The Ownership Engine populates this before IR emission and the
 *  IR preserves it so the backend knows exactly where to insert drops.
 * ============================================================ */

typedef enum {
    OWN_OWNED,          /* this instruction holds full ownership              */
    OWN_MOVED,          /* value has been moved out — no longer accessible    */
    OWN_BORROWED,       /* shared borrow (&T) — read-only, no ownership       */
    OWN_BORROWED_MUT,   /* exclusive borrow (&mut T) — can mutate             */
    OWN_DROPPED,        /* destructor already called — dead value              */
} IrOwnershipState;

struct IrOwnership {
    IrValueId       value_id;
    IrOwnershipState state;
    IrLifetimeId    lifetime;       /* region during which this state is valid  */
    IrBlockId       moved_in_block; /* if MOVED, where the move happened        */
    IrBlockId       drop_in_block;  /* if DROPPED, where destructor is inserted */
    bool            is_partial;     /* partial move (one field of a struct moved) */
};

/* ============================================================
 *  §5  Resource Tracking
 *
 *  Resources are non-memory OS handles: files, sockets, mutexes, etc.
 *  The Resource Checker gives every acquire an IrResource entry.
 *  The backend uses this to insert the correct close/release calls.
 * ============================================================ */

typedef enum {
    RES_FILE,
    RES_SOCKET_TCP,
    RES_SOCKET_UDP,
    RES_MUTEX,
    RES_THREAD,
    RES_GPU_BUFFER,
    RES_CUSTOM,         /* user-defined via #[resource] trait impl  */
} IrResourceKind;

typedef enum {
    RES_STATE_ACQUIRED,
    RES_STATE_RELEASED,
    RES_STATE_POSSIBLY_RELEASED,   /* after conditional release (if branch) */
    RES_STATE_LEAKED,              /* error: resource left open — compile failure */
} IrResourceState;

struct IrResource {
    IrResourceId    id;
    IrResourceKind  kind;
    IrResourceState state;
    IrValueId       handle_value;  /* the SSA value holding the handle */
    IrLifetimeId    lifetime;
    IrBlockId       acquire_block;
    IrBlockId       release_block; /* IR_ID_INVALID = not yet released (error) */
    const char     *debug_name;    /* e.g. "file:data.txt" */
};

/* ============================================================
 *  §6  Concurrency Effect Annotations
 *
 *  The Effect System tracks which values are shared across task
 *  boundaries so the Concurrency Checker can reject data races
 *  before any code is generated.
 * ============================================================ */

typedef enum {
    EFF_PURE,          /* no external effects — safe for compile-time eval */
    EFF_READ,          /* reads shared state                               */
    EFF_WRITE,         /* writes shared state                              */
    EFF_SEND,          /* value crosses task boundary                      */
    EFF_SYNC,          /* acquires or releases a synchronization primitive */
    EFF_IO,            /* file/network I/O                                 */
    EFF_PANIC,         /* may panic / terminate                            */
} IrEffectKind;

struct IrEffect {
    IrEffectKind  kind;
    IrValueId     operand;    /* the value involved in the effect (if any)   */
    u32           task_id;    /* which spawned task produces this effect      */
};

/* ============================================================
 *  §7  Instructions (SSA Nodes)
 *
 *  Every IrInstr produces exactly one IrValue (its result).
 *  For void instructions (stores, branches), result_type = IR_TY_VOID
 *  and result_id = IR_ID_INVALID.
 * ============================================================ */

typedef enum {
    /* --- Literals --- */
    IR_CONST_INT,         /* i64 immediate                              */
    IR_CONST_FLOAT,       /* f64 immediate                              */
    IR_CONST_BOOL,        /* 0 or 1                                     */
    IR_CONST_STR,         /* pointer to interned UTF-8 string literal   */
    IR_CONST_NULL,        /* typed null pointer                         */

    /* --- Memory --- */
    IR_ALLOCA,            /* stack-allocate N bytes → ptr               */
    IR_HEAP_ALLOC,        /* heap-allocate (VirtualAlloc) → ptr         */
    IR_ARENA_ALLOC,       /* arena-allocate → ptr                       */
    IR_LOAD,              /* *ptr → value                               */
    IR_STORE,             /* *ptr = value (void)                        */
    IR_GEP,               /* GetElementPtr: base + offset → ptr         */
    IR_DROP,              /* insert destructor call for value            */
    IR_MOVE,              /* transfer ownership of an SSA value          */

    /* --- Arithmetic (num / flt / dob) --- */
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_REM,
    IR_NEG,
    IR_FADD,
    IR_FSUB,
    IR_FMUL,
    IR_FDIV,

    /* --- Bitwise --- */
    IR_AND,
    IR_OR,
    IR_XOR,
    IR_SHL,
    IR_SHR,               /* arithmetic (signed) right shift            */
    IR_NOT,

    /* --- Comparison --- */
    IR_CMP_EQ,
    IR_CMP_NE,
    IR_CMP_LT,
    IR_CMP_LE,
    IR_CMP_GT,
    IR_CMP_GE,

    /* --- Type Casts --- */
    IR_CAST_INT_TO_FLOAT,
    IR_CAST_FLOAT_TO_INT,
    IR_CAST_WIDEN,        /* zero- or sign-extend to wider int          */
    IR_CAST_TRUNCATE,
    IR_BITCAST,           /* reinterpret bits — only in unsafe { }       */

    /* --- Control Flow --- */
    IR_JUMP,              /* unconditional branch → block               */
    IR_BRANCH,            /* conditional branch: cond → then/else block  */
    IR_SWITCH,            /* pattern-match branch table                  */
    IR_RETURN,            /* return value from function                  */
    IR_PANIC,             /* runtime panic — print message + exit        */

    /* --- Functions --- */
    IR_CALL,              /* direct function call                        */
    IR_CALL_INDIRECT,     /* call via function pointer                   */
    IR_TAIL_CALL,         /* tail-recursive call (no new stack frame)    */

    /* --- Concurrency --- */
    IR_SPAWN,             /* spawn a task — returns task handle          */
    IR_AWAIT,             /* await a task handle — returns result value  */
    IR_CHAN_SEND,         /* send value into a channel                   */
    IR_CHAN_RECV,         /* receive value from a channel                */
    IR_MUTEX_LOCK,        /* acquire mutex                               */
    IR_MUTEX_UNLOCK,      /* release mutex                               */

    /* --- Resource Lifecycle --- */
    IR_RESOURCE_ACQUIRE,  /* open file / socket / etc.                   */
    IR_RESOURCE_RELEASE,  /* close file / socket / etc.                  */

    /* --- Compile-Time / Meta --- */
    IR_COMPTIME,          /* marker: this subtree was folded at CT       */
    IR_INLINE_ASM,        /* raw assembly block                          */
    IR_EXTERN_CALL,       /* call into C FFI function                    */
    IR_PHI,               /* SSA φ-node — merge values from predecessors */
    IR_UNDEF,             /* explicitly undefined (for dead-code paths)  */

    IR_OP_COUNT,
} IrOpCode;

/*
 * IrOperand — one operand of an instruction.
 * Either a reference to another SSA value or an inline immediate.
 */
typedef struct {
    bool       is_imm;        /* true = immediate; false = value reference */
    union {
        IrValueId  val;       /* SSA value reference                        */
        i64        int_imm;   /* integer immediate                          */
        double     flt_imm;   /* float/double immediate                     */
        const char *str_imm;  /* interned string literal pointer            */
    };
} IrOperand;

/*
 * IrInstr — one SSA instruction.
 *
 * Key fields that go beyond standard LLVM IR:
 *   `ownership`  — ownership state of the result value
 *   `lifetime`   — lifetime region of the result value
 *   `resource`   — if this instruction acquires/releases a resource
 *   `effects`    — concurrency effects produced by this instruction
 *   `is_unsafe`  — true if instruction is inside an `unsafe { }` block
 */
struct IrInstr {
    IrOpCode    op;
    IrValueId   result_id;        /* SSA result (IR_ID_INVALID for void ops)  */
    IrTypeId    result_type;

    IrOperand  *operands;         /* input operands                            */
    u32         operand_count;

    /* --- Ownership & Lifetime Metadata --- */
    IrOwnership ownership;        /* who owns the result                      */
    IrLifetimeId result_lifetime; /* region the result is alive               */

    /* --- Resource Metadata --- */
    IrResourceId resource_id;     /* IR_ID_INVALID if not a resource op       */

    /* --- Effect Metadata --- */
    IrEffect   *effects;          /* side-effects of this instruction          */
    u32         effect_count;

    /* --- Debug Info --- */
    u32         src_line;
    u32         src_col;
    const char *src_file;

    /* --- Flags --- */
    bool        is_unsafe;        /* inside unsafe { } block                  */
    bool        is_comptime;      /* fully folded at compile time              */
    bool        may_panic;        /* can this instruction trigger a panic?     */
    bool        is_tail;          /* IR_CALL eligible for tail-call elim.      */
};

/* ============================================================
 *  §8  Basic Block
 * ============================================================ */

/*
 * IrBlock — a maximal sequence of instructions with no branches
 *           except at the very end (the terminator).
 *
 * Every block ends in exactly one of:
 *   IR_JUMP, IR_BRANCH, IR_SWITCH, IR_RETURN, or IR_PANIC.
 */
struct IrBlock {
    IrBlockId    id;
    const char  *label;           /* debug name, e.g. "entry", "loop.body"     */

    IrInstr     *instrs;          /* instruction array (SSA ordered)           */
    u32          instr_count;
    u32          instr_capacity;

    /* CFG edges */
    IrBlockId   *predecessors;
    u32          pred_count;

    IrBlockId   *successors;
    u32          succ_count;

    /* Dominance info (populated by the dominance pass) */
    IrBlockId    idom;            /* immediate dominator                        */
    u32          dom_depth;

    /* Lifetime scopes that begin or end in this block */
    IrLifetimeId *lifetimes_begin;
    u32           lifetimes_begin_count;

    IrLifetimeId *lifetimes_end;
    u32           lifetimes_end_count;
};

/* ============================================================
 *  §9  Function
 * ============================================================ */

typedef enum {
    FN_NORMAL,
    FN_EXTERN,       /* C FFI declaration                               */
    FN_COMPTIME,     /* evaluated entirely at compile time              */
    FN_CLOSURE,      /* captures an environment                        */
    FN_TASK_ENTRY,   /* entry point of a spawned task                  */
} IrFunctionKind;

/*
 * IrParam — a function parameter, including borrow/mutability metadata.
 */
typedef struct {
    IrValueId    value_id;
    IrTypeId     type_id;
    const char  *name;
    bool         is_mut;          /* `mut` parameter                          */
    bool         is_self;         /* receiver parameter (for trait impl)      */
    IrOwnership  ownership;
    IrLifetimeId lifetime;
} IrParam;

/*
 * IrFunction — one RDX function, fully lowered to SSA blocks.
 */
struct IrFunction {
    const char     *name;
    IrFunctionKind  kind;

    IrParam        *params;
    u32             param_count;

    IrTypeId        return_type;

    IrBlock        *blocks;       /* ordered: blocks[0] is always the entry   */
    u32             block_count;

    /* All lifetimes declared in this function */
    IrLifetime     *lifetimes;
    u32             lifetime_count;

    /* All resources tracked in this function */
    IrResource     *resources;
    u32             resource_count;

    /* Effects declared for the function as a whole (for trait dispatch) */
    IrEffect       *effects;
    u32             effect_count;

    /* SSA value table: maps IrValueId → IrInstr* that defines it */
    IrInstr       **value_defs;   /* indexed by IrValueId                     */
    u32             value_count;

    /* Closure environment — NULL for non-closures */
    IrTypeId        env_type;     /* struct containing captured variables     */
    bool            env_by_move;  /* true = move capture, false = borrow      */

    /* Generic instantiation info */
    IrTypeId       *type_args;    /* concrete types substituted for generics  */
    u32             type_arg_count;

    /* Compiler flags */
    bool            is_pub;
    bool            is_inline;
    bool            is_unsafe;
    bool            no_return;    /* function never returns (panic, exit, …)  */
};

/* ============================================================
 *  §10  Module (Top-Level IR Container)
 * ============================================================ */

/*
 * IrGlobal — a module-level constant or static variable.
 */
typedef struct {
    const char  *name;
    IrTypeId     type_id;
    bool         is_const;        /* compile-time constant (cannot mutate)    */
    bool         is_pub;
    IrOperand    initializer;     /* must be a compile-time constant operand  */
} IrGlobal;

/*
 * IrImport — one resolved `use` statement.
 */
typedef struct {
    const char  *module_path;     /* e.g. "std.io"                            */
    const char  *symbol;          /* imported name, or "*" for glob           */
    bool         is_pub;          /* re-exported with `pub use`?              */
} IrImport;

/*
 * IrModule — the top-level container for one compiled .rdx file.
 */
struct IrModule {
    const char   *name;           /* module name (file path without .rdx)     */
    const char   *source_path;    /* original .rdx file path                  */

    IrFunction   *functions;
    u32           function_count;

    IrGlobal     *globals;
    u32           global_count;

    IrImport     *imports;
    u32           import_count;

    /* Interned type table — all IrType objects in this module */
    IrType       *types;
    u32           type_count;

    /* String interning table — deduplicated string literals */
    const char  **string_pool;
    u32           string_count;

    /* Compiler metadata */
    u32           ir_version;     /* RDX-IR format version for caching        */
    bool          has_unsafe;     /* true = module contains unsafe { } blocks */
    bool          has_comptime;   /* true = module has compile-time functions  */
};

/* ============================================================
 *  §11  IR Builder API  (declared here, defined in rdx_ir.c)
 *
 *  The IRBuilder is the only interface that creates IrInstr nodes.
 *  All allocations go into the bump allocator (rdx_alloc_bump).
 * ============================================================ */

/* Creates a new empty module */
IrModule    *ir_module_new(const char *name, const char *source_path);

/* Creates a new function in a module */
IrFunction  *ir_func_new(IrModule *mod, const char *name, IrFunctionKind kind);

/* Creates a new basic block inside a function */
IrBlock     *ir_block_new(IrFunction *fn, const char *label);

/* Emits an instruction into a block; returns the result value ID */
IrValueId    ir_emit(IrBlock *blk, IrOpCode op,
                     IrTypeId result_type,
                     IrOperand *operands, u32 operand_count,
                     IrLifetimeId lifetime, IrOwnershipState own_state);

/* Convenience emitters */
IrValueId    ir_emit_const_int(IrBlock *blk, i64 val);
IrValueId    ir_emit_const_flt(IrBlock *blk, double val);
IrValueId    ir_emit_const_str(IrBlock *blk, const char *str);
IrValueId    ir_emit_add(IrBlock *blk, IrValueId lhs, IrValueId rhs, IrTypeId ty);
IrValueId    ir_emit_call(IrBlock *blk, IrValueId fn_val,
                          IrValueId *args, u32 argc, IrTypeId ret_ty);
void         ir_emit_return(IrBlock *blk, IrValueId val);
void         ir_emit_branch(IrBlock *blk, IrValueId cond,
                             IrBlockId then_blk, IrBlockId else_blk);
void         ir_emit_jump(IrBlock *blk, IrBlockId target);

/* Resource tracking helpers */
IrResourceId ir_resource_acquire(IrFunction *fn, IrResourceKind kind,
                                  IrValueId handle, IrLifetimeId lt,
                                  const char *debug_name);
void         ir_resource_release(IrFunction *fn, IrResourceId res_id,
                                  IrBlockId release_block);

/* Ownership / move helpers */
void         ir_mark_moved(IrFunction *fn, IrValueId val, IrBlockId moved_in);
void         ir_insert_drop(IrBlock *blk, IrValueId val);

/* Dump IR to stdout (debug) */
void         ir_module_dump(const IrModule *mod);
void         ir_function_dump(const IrFunction *fn);

/* Validate IR consistency (ownership, resources, lifetimes) */
bool         ir_module_validate(const IrModule *mod);

#endif /* RDX_IR_H */
