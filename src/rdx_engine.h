/*
 * ============================================================================
 *  rdx_engine.h  —  RDX Syntax-Free Execution Engine & Cyber Forensics Runtime
 * ============================================================================
 */

#ifndef RDX_ENGINE_H
#define RDX_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STR,
    VAL_LIST,
    VAL_FN
} ValType;

struct RdxVal;

typedef struct {
    struct RdxVal *items;
    int            count;
    int            capacity;
} RdxList;

typedef struct {
    char         name[64];
    char         params[8][64];
    int          param_count;
    int          body_stmt_start;
    int          body_stmt_count;
} RdxFunc;

typedef struct RdxVal {
    ValType type;
    union {
        bool    bval;
        int64_t ival;
        double  fval;
        char   *sval;
        RdxList*lval;
        RdxFunc*fnval;
    } as;
} RdxVal;

/* Scope / Symbol Environment */
typedef struct {
    char   name[64];
    RdxVal val;
} RdxVar;

typedef struct RdxScope {
    RdxVar           vars[256];
    int              var_count;
    struct RdxScope *parent;
} RdxScope;

/* Public API */
int  rdx_engine_run(const char *filename, const char *source);
void rdx_engine_repl(void);

#endif /* RDX_ENGINE_H */
