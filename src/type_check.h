/*
 * ============================================================================
 *  type_check.h  —  RDX Static Type Checker (v2.0)
 * ============================================================================
 *
 *  Phase 3c: Full static type system sitting between the parser and the
 *  bytecode compiler.
 *
 *  Responsibilities:
 *    - Infer types for all expressions and variable declarations
 *    - Type-check function call arguments against signatures
 *    - Resolve struct/enum member access
 *    - Check generics (monomorphization recorded for the backend)
 *    - Verify match exhaustiveness
 *    - Check return types match function declaration
 *    - Flag type mismatches as compile errors
 *
 *  This is a SEPARATE pass from the ownership analyzer (analyzer.c).
 *  The type checker does NOT track ownership; it only tracks types.
 */

#ifndef RDX_TYPE_CHECK_H
#define RDX_TYPE_CHECK_H

#include "core.h"
#include "arena.h"
#include "ast.h"
#include "diag.h"

/* ============================================================
 *  RDX Type Descriptors
 * ============================================================ */

typedef enum {
    TY_UNKNOWN = 0,  /* Not yet inferred                                */
    TY_NIL,          /* nil / unit                                      */
    TY_BOOL,         /* blo                                             */
    TY_INT,          /* num — i64                                       */
    TY_FLOAT,        /* flt — f32                                       */
    TY_DOUBLE,       /* dob — f64                                       */
    TY_BYTE,         /* byt — u8                                        */
    TY_STR,          /* chr — UTF-8 string                              */
    TY_PTR,          /* ptr — unsafe raw pointer                        */
    TY_ARRAY,        /* []T                                             */
    TY_SLICE,        /* &[]T                                            */
    TY_TUPLE,        /* (T0, T1, …)                                     */
    TY_STRUCT,       /* user-defined struct                             */
    TY_ENUM,         /* user-defined enum                               */
    TY_FUNCTION,     /* fn(A,B,...) -> R                                */
    TY_CLOSURE,      /* closure fn(A,B,...) -> R                        */
    TY_RESULT,       /* Result[T]                                       */
    TY_OPTION,       /* Option[T]                                       */
    TY_NEVER,        /* ! — functions that never return                 */
    TY_GENERIC,      /* unresolved generic parameter T                  */
    TY_INFER,        /* _ — let the type checker fill this in           */
} RdxTypeKind;

#define MAX_TYPE_PARAMS   8
#define MAX_FN_PARAMS    64
#define MAX_STRUCT_FIELDS 64
#define MAX_ENUM_VARIANTS 64

typedef struct RdxType RdxType;

struct RdxType {
    RdxTypeKind kind;
    const char *name;            /* for STRUCT, ENUM, GENERIC: the name      */
    usize       name_len;

    /* Inner types (for ARRAY, OPTION, RESULT, TUPLE, FN, etc.)              */
    RdxType    *inner[MAX_TYPE_PARAMS];  /* inner[0] = element type for ARRAY */
    u32         inner_count;

    /* For FUNCTION/CLOSURE: parameter types + return type                   */
    RdxType    *param_types[MAX_FN_PARAMS];
    u32         param_count;
    RdxType    *return_type;

    /* For STRUCT: field names + types                                        */
    struct {
        const char *name;
        usize       name_len;
        RdxType    *type;
    } fields[MAX_STRUCT_FIELDS];
    u32 field_count;

    /* For ENUM: variant names + payload types                                */
    struct {
        const char *name;
        usize       name_len;
        u32         tag;
        RdxType    *payload[MAX_TYPE_PARAMS];
        u32         payload_count;
    } variants[MAX_ENUM_VARIANTS];
    u32 variant_count;
};

/* ============================================================
 *  Type Environment (symbol table for type-checking)
 * ============================================================ */

#define TC_MAX_SCOPES  64
#define TC_MAX_SYMS   512
#define TC_MAX_TYPES  256

typedef struct {
    const char *name;
    usize       name_len;
    RdxType    *type;
    bool        is_mut;
} TcSymbol;

typedef struct {
    u32   base;   /* first symbol index in this scope */
    u32   count;  /* number of symbols in this scope  */
} TcScope;

typedef struct {
    /* Symbol table */
    TcSymbol  symbols[TC_MAX_SYMS];
    u32       sym_count;

    /* Scope stack */
    TcScope   scopes[TC_MAX_SCOPES];
    u32       scope_depth;

    /* Named type registry (struct / enum definitions) */
    RdxType  *named_types[TC_MAX_TYPES];
    u32       named_type_count;

    /* Current function return type (for checking ret statements) */
    RdxType  *current_fn_return;
    bool      in_loop;           /* for break/continue checks */

    /* Error tracking */
    bool      had_error;
    u32       error_count;

    Arena    *arena;
} TypeChecker;

/* ============================================================
 *  Public API
 * ============================================================ */

/* Initialize a type checker */
void tc_init(TypeChecker *tc, Arena *arena);

/* Type-check a complete program.
 * Returns TRUE if no errors, FALSE if type errors were found. */
bool tc_check_program(TypeChecker *tc, AstProgram *prog);

/* Infer the type of an expression (does NOT flag errors — use tc_check_expr) */
RdxType *tc_infer_expr(TypeChecker *tc, AstExpr *expr);

/* Check an expression and return its type (flags errors on mismatch) */
RdxType *tc_check_expr(TypeChecker *tc, AstExpr *expr, RdxType *expected);

/* Look up a named type by name */
RdxType *tc_find_type(TypeChecker *tc, const char *name, usize len);

/* Built-in primitive types (pre-allocated, shared) */
extern RdxType TC_TY_NIL;
extern RdxType TC_TY_BOOL;
extern RdxType TC_TY_INT;
extern RdxType TC_TY_FLOAT;
extern RdxType TC_TY_DOUBLE;
extern RdxType TC_TY_BYTE;
extern RdxType TC_TY_STR;
extern RdxType TC_TY_NEVER;
extern RdxType TC_TY_UNKNOWN;

/* Construct compound types */
RdxType *tc_make_array(Arena *arena, RdxType *elem);
RdxType *tc_make_option(Arena *arena, RdxType *inner);
RdxType *tc_make_result(Arena *arena, RdxType *ok_type);
RdxType *tc_make_tuple(Arena *arena, RdxType **elems, u32 count);
RdxType *tc_make_fn(Arena *arena, RdxType **params, u32 param_count, RdxType *ret);

/* Type compatibility check */
bool tc_types_compatible(const RdxType *a, const RdxType *b);
bool tc_type_is_numeric(const RdxType *t);
bool tc_type_is_copy(const RdxType *t);

/* Pretty-print a type for diagnostic messages */
void tc_print_type(const RdxType *t);

#endif /* RDX_TYPE_CHECK_H */
