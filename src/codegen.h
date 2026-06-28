/* codegen.h — RDX Win64 NASM Code Generator: Public API */
#ifndef RDX_CODEGEN_H
#define RDX_CODEGEN_H
#include "parser.h"

/* Emit buffer — holds the generated NASM text */
typedef struct {
    char  *buf;
    usize  pos;
    usize  cap;
} EmitBuf;

/* Local variable entry in a function frame */
typedef struct { char name[64]; char rdx_type[64]; int offset; } Local;

/* Blueprint field layout */
typedef struct {
    char name[64];
    char field_names[32][64];
    char field_types[32][64];
    int  field_count;
} Blueprint;

/* Code-generator state */
typedef struct {
    EmitBuf    out;
    /* string literals collected for .data section */
    char       strs[512][256];   /* string literal pool — 128 KiB */
    int        str_count;
    /* blueprint registry */
    Blueprint  bps[32];
    int        bp_count;
    /* per-function locals */
    Local      locals[128];
    int        local_count;
    int        frame_size;
    /* label counter for if / loop */
    int        label_id;
    int        had_error;
} CodeGen;

/* Run codegen over the AST.  Fills cg->out with NASM text.
   Returns 0 on success, -1 on error. */
int codegen_run(CodeGen *cg, int root_node);

/* Initialise a CodeGen (allocates emit buffer from bump heap). */
void codegen_init(CodeGen *cg);
#endif
