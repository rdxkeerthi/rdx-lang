/* codegen.h — RDX Direct Machine-Code JIT Engine */
#ifndef RDX_CODEGEN_H
#define RDX_CODEGEN_H
#include "parser.h"

/* Simple symbol table for variables during JIT execution */
typedef struct {
    char name[64];
    char type[64]; /* "num", "chr", "flt", "blo", "dob" */
    
    /* values */
    i64  ival;
    double dval;
    char sval[256];
} JitVar;

typedef struct {
    JitVar vars[128];
    int    var_count;
    int    had_error;
    int    continue_flag;  /* set by NODE_CONTINUE — loop skips to next iter */
    int    break_flag;     /* set by break sentinel (NODE_RETURN ival==-999)  */
} JitEngine;

/* Run the JIT execution engine directly on the AST root. */
int jit_run(JitEngine *jit, int root_node);

void jit_init(JitEngine *jit);

#endif
