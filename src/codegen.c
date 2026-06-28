/* codegen.c — RDX Win64 NASM Code Generator
   Emits x86-64 assembly using Microsoft x64 ABI + kernel32.dll
   Deps: codegen.h -> parser.h -> lexer.h -> core.h */
#include "codegen.h"

/* ============================================================
 *  §1  Emit helpers
 * ============================================================ */
static void em(CodeGen *cg, const char *s) {
    usize len = my_strlen(s);
    if (cg->out.pos + len >= cg->out.cap) { cg->had_error = 1; return; }
    my_memcpy(cg->out.buf + cg->out.pos, s, len);
    cg->out.pos += len;
}
static void em_num(CodeGen *cg, i64 v) {
    char buf[24]; char *p = buf + 22; *p = '\0';
    bool neg = (v < 0);
    u64 u = neg ? (u64)(-(v+1))+1ULL : (u64)v;
    do { *--p = '0' + (u % 10); u /= 10; } while (u);
    if (neg) *--p = '-';
    em(cg, p);
}
static void em_line(CodeGen *cg, const char *s) { em(cg, s); em(cg, "\n"); }
static void em_label(CodeGen *cg, const char *pfx, int id, const char *sfx) {
    em(cg, pfx); em_num(cg, id); em(cg, sfx);
}

/* ============================================================
 *  §2  Symbol table helpers
 * ============================================================ */
static int sym_find(CodeGen *cg, const char *name) {
    for (int i = 0; i < cg->local_count; i++)
        if (my_strcmp(cg->locals[i].name, name) == 0)
            return cg->locals[i].offset;
    return 0; /* not found — offset 0 is always rbp, safe sentinel */
}
static int sym_add(CodeGen *cg, const char *name, const char *type) {
    cg->frame_size += 8;
    Local *l = &cg->locals[cg->local_count++];
    my_strncpy(l->name, name, 63);
    my_strncpy(l->rdx_type, type, 63);
    l->offset = -(cg->frame_size);
    return l->offset;
}
static void sym_clear(CodeGen *cg) { cg->local_count = 0; cg->frame_size = 0; }

/* ============================================================
 *  §3  Blueprint helpers
 * ============================================================ */
static Blueprint *bp_find(CodeGen *cg, const char *name) {
    for (int i = 0; i < cg->bp_count; i++)
        if (my_strcmp(cg->bps[i].name, name) == 0)
            return &cg->bps[i];
    return NULL;
}
static int bp_field_offset(Blueprint *bp, const char *field) {
    for (int i = 0; i < bp->field_count; i++)
        if (my_strcmp(bp->field_names[i], field) == 0)
            return i * 8;
    return 0;
}
static int bp_size(Blueprint *bp) { return bp->field_count * 8; }

/* ============================================================
 *  §4  String literal pool
 * ============================================================ */
static int str_intern(CodeGen *cg, const char *s) {
    /* Check if already exists */
    for (int i = 0; i < cg->str_count; i++)
        if (my_strcmp(cg->strs[i], s) == 0) return i;
    my_strncpy(cg->strs[cg->str_count], s, 4095);
    return cg->str_count++;
}

/* ============================================================
 *  §5  Win64 frame size calculation
 *      Must be: >= 32 (shadow) + locals, aligned to 16
 * ============================================================ */
static int calc_frame(int locals_bytes) {
    int total = 32 + locals_bytes + 8; /* +8 for 5th-arg WriteFile slot */
    /* round up to multiple of 16 */
    return (total + 15) & ~15;
}

/* ============================================================
 *  §6  Expression code generation
 *      Result always ends up in rax.
 * ============================================================ */
static void gen_expr(CodeGen *cg, int idx);

static void gen_expr(CodeGen *cg, int idx) {
    if (idx < 0) return;
    ASTNode *n = &rdx_nodes[idx];

    switch (n->type) {
    case NODE_LITERAL_NUM:
        em(cg, "    mov rax, "); em_num(cg, n->ival); em(cg, "\n");
        break;

    case NODE_LITERAL_STR: {
        int si = str_intern(cg, n->sval);
        em(cg, "    lea rax, [rel _str"); em_num(cg, si); em(cg, "]\n");
        break;
    }

    case NODE_IDENT: {
        int off = sym_find(cg, n->sval);
        if (off == 0) {
            em(cg, "    ; WARN: undefined var "); em_line(cg, n->sval);
            em_line(cg, "    xor rax, rax");
        } else {
            em(cg, "    mov rax, qword [rbp"); em_num(cg, off); em(cg, "]\n");
        }
        break;
    }

    case NODE_MEMBER_ACCESS: {
        /* sval = object, rdx_type = field */
        int off = sym_find(cg, n->sval);
        Blueprint *bp = bp_find(cg, n->sval);
        int foff = bp ? bp_field_offset(bp, n->rdx_type) : 0;
        em(cg, "    mov rax, qword [rbp"); em_num(cg, off); em(cg, "]\n");
        em(cg, "    mov rax, qword [rax+"); em_num(cg, foff); em(cg, "]\n");
        break;
    }

    case NODE_ALLOC: {
        /* VirtualAlloc(NULL, size, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE) */
        Blueprint *bp = bp_find(cg, n->rdx_type);
        int sz = bp ? bp_size(bp) : 16;
        em_line(cg, "    xor  rcx, rcx");
        em(cg, "    mov  rdx, "); em_num(cg, sz); em(cg, "\n");
        em_line(cg, "    mov  r8d, 0x3000");
        em_line(cg, "    mov  r9d, 0x04");
        em_line(cg, "    call VirtualAlloc");
        break;
    }

    case NODE_BINARY_OP: {
        /* Evaluate right into rcx, left into rax, then combine */
        gen_expr(cg, n->left);
        em_line(cg, "    push rax");
        gen_expr(cg, n->right);
        em_line(cg, "    mov  rcx, rax");
        em_line(cg, "    pop  rax");
        if      (my_strcmp(n->op, "+")  == 0) em_line(cg, "    add  rax, rcx");
        else if (my_strcmp(n->op, "-")  == 0) em_line(cg, "    sub  rax, rcx");
        else if (my_strcmp(n->op, "*")  == 0) em_line(cg, "    imul rax, rcx");
        else if (my_strcmp(n->op, "/")  == 0) {
            em_line(cg, "    cqo");
            em_line(cg, "    idiv rcx");
        }
        else if (my_strcmp(n->op, "==") == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    sete al");
            em_line(cg, "    movzx rax, al");
        }
        else if (my_strcmp(n->op, "!=") == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    setne al");
            em_line(cg, "    movzx rax, al");
        }
        else if (my_strcmp(n->op, "<")  == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    setl al");
            em_line(cg, "    movzx rax, al");
        }
        else if (my_strcmp(n->op, ">")  == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    setg al");
            em_line(cg, "    movzx rax, al");
        }
        else if (my_strcmp(n->op, "<=") == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    setle al");
            em_line(cg, "    movzx rax, al");
        }
        else if (my_strcmp(n->op, ">=") == 0) {
            em_line(cg, "    cmp  rax, rcx");
            em_line(cg, "    setge al");
            em_line(cg, "    movzx rax, al");
        }
        break;
    }

    case NODE_FN_CALL: {
        /* Push args right-to-left, then move into registers */
        /* V1: up to 4 args via rcx/rdx/r8/r9 */
        int argc = n->child_count;
        for (int i = 0; i < argc; i++) {
            gen_expr(cg, n->children[i]);
            em_line(cg, "    push rax");
        }
        static const char *regs[] = {"rcx","rdx","r8","r9"};
        for (int i = argc - 1; i >= 0; i--) {
            if (i < 4) { em(cg,"    pop  "); em_line(cg, regs[i]); }
            else        em_line(cg,"    pop  rax  ; extra arg discarded");
        }
        em(cg, "    call rdx_fn_"); em_line(cg, n->sval);
        break;
    }

    default:
        em_line(cg, "    xor rax, rax ; unhandled expr");
        break;
    }
}

/* ============================================================
 *  §7  Condition generation for if / loop
 *      Emits CMP + conditional JMP to `false_label`.
 * ============================================================ */
static void gen_cond(CodeGen *cg, int cond_idx, const char *false_label) {
    ASTNode *c = &rdx_nodes[cond_idx];
    if (c->type == NODE_BINARY_OP) {
        /* Evaluate left → rax, right → rcx, then cmp */
        gen_expr(cg, c->left);
        em_line(cg, "    push rax");
        gen_expr(cg, c->right);
        em_line(cg, "    mov  rcx, rax");
        em_line(cg, "    pop  rax");
        em_line(cg, "    cmp  rax, rcx");
        const char *jmp;
        if      (my_strcmp(c->op, "==") == 0) jmp = "    jne ";
        else if (my_strcmp(c->op, "!=") == 0) jmp = "    je  ";
        else if (my_strcmp(c->op, "<")  == 0) jmp = "    jge ";
        else if (my_strcmp(c->op, ">")  == 0) jmp = "    jle ";
        else if (my_strcmp(c->op, "<=") == 0) jmp = "    jg  ";
        else if (my_strcmp(c->op, ">=") == 0) jmp = "    jl  ";
        else                                   jmp = "    je  ";
        em(cg, jmp); em_line(cg, false_label);
    } else {
        /* Generic: evaluate, test rax == 0 */
        gen_expr(cg, cond_idx);
        em_line(cg, "    test rax, rax");
        em(cg, "    jz "); em_line(cg, false_label);
    }
}

/* ============================================================
 *  §8  Statement code generation
 * ============================================================ */
static void gen_stmt(CodeGen *cg, int idx);

static void gen_block(CodeGen *cg, int idx) {
    if (idx < 0) return;
    ASTNode *blk = &rdx_nodes[idx];
    for (int i = 0; i < blk->child_count; i++)
        gen_stmt(cg, blk->children[i]);
}

static void gen_stmt(CodeGen *cg, int idx) {
    if (idx < 0) return;
    ASTNode *n = &rdx_nodes[idx];

    switch (n->type) {

    case NODE_VAR_DECL: {
        int off = sym_add(cg, n->sval, n->rdx_type);
        gen_expr(cg, n->left);
        em(cg, "    mov  qword [rbp"); em_num(cg, off); em(cg, "], rax\n");
        break;
    }

    case NODE_ASSIGN: {
        gen_expr(cg, n->left);
        /* Member assignment: sval=obj, rdx_type=field */
        if (n->rdx_type[0] != '\0' && my_strcmp(n->rdx_type, "num") != 0
                                    && my_strcmp(n->rdx_type, "str") != 0) {
            /* Looks like a field assignment */
            int obj_off = sym_find(cg, n->sval);
            em_line(cg, "    push rax");
            em(cg, "    mov  rcx, qword [rbp"); em_num(cg, obj_off); em(cg, "]\n");
            em_line(cg, "    pop  rax");
            /* TODO: compute field offset from blueprint */
            em_line(cg, "    mov  qword [rcx+0], rax ; field assign");
        } else {
            int off = sym_find(cg, n->sval);
            em(cg, "    mov  qword [rbp"); em_num(cg, off); em(cg, "], rax\n");
        }
        break;
    }

    case NODE_RETURN:
        gen_expr(cg, n->left);
        /* Epilogue is emitted by caller; just jump to it */
        em_line(cg, "    jmp  .fn_ret");
        break;

    case NODE_DROP: {
        int off = sym_find(cg, n->sval);
        em(cg, "    mov  rcx, qword [rbp"); em_num(cg, off); em(cg, "]\n");
        em_line(cg, "    xor  rdx, rdx");
        em_line(cg, "    mov  r8d, 0x8000");
        em_line(cg, "    call VirtualFree");
        break;
    }

    case NODE_IF: {
        int id = cg->label_id++;
        /* label buffers */
        char lbl_else[32], lbl_end[32];
        my_strcpy(lbl_else, "_if"); /* build: _ifN_else */
        {
            char tmp[16]; char *p = tmp+14; *p='\0';
            int v = id; do {*--p='0'+(v%10);v/=10;}while(v);
            my_strcat(lbl_else, p);
        }
        my_strcat(lbl_else, "_else");
        my_strcpy(lbl_end, "_if");
        {
            char tmp[16]; char *p = tmp+14; *p='\0';
            int v = id; do {*--p='0'+(v%10);v/=10;}while(v);
            my_strcat(lbl_end, p);
        }
        my_strcat(lbl_end, "_end");

        gen_cond(cg, n->condition, lbl_else);
        gen_block(cg, n->body);
        if (n->else_body >= 0) {
            em(cg, "    jmp  "); em_line(cg, lbl_end);
        }
        em(cg, lbl_else); em_line(cg, ":");
        if (n->else_body >= 0) {
            gen_block(cg, n->else_body);
        }
        em(cg, lbl_end); em_line(cg, ":");
        break;
    }

    case NODE_LOOP: {
        int id = cg->label_id++;
        char lbl_start[32], lbl_end[32];
        my_strcpy(lbl_start, "_loop");
        my_strcpy(lbl_end,   "_loop");
        {
            char tmp[16]; char *p = tmp+14; *p='\0';
            int v=id; do{*--p='0'+(v%10);v/=10;}while(v);
            my_strcat(lbl_start, p);
            my_strcat(lbl_end,   p);
        }
        my_strcat(lbl_start, "_start");
        my_strcat(lbl_end,   "_end");

        em(cg, lbl_start); em_line(cg, ":");
        gen_cond(cg, n->condition, lbl_end);
        gen_block(cg, n->body);
        em(cg, "    jmp  "); em_line(cg, lbl_start);
        em(cg, lbl_end);   em_line(cg, ":");
        break;
    }

    case NODE_SYS_CALL: {
        /* call("string") → WriteFile(stdout, ptr, len, &written, NULL) */
        int si = str_intern(cg, n->sval);
        em_line(cg, "    mov  rcx, [rel _rdx_stdout]");
        em(cg, "    lea  rdx, [rel _str"); em_num(cg, si); em(cg, "]\n");
        em(cg, "    mov  r8d, _str"); em_num(cg, si); em(cg, "_len\n");
        em_line(cg, "    lea  r9,  [rel _rdx_written]");
        em_line(cg, "    mov  qword [rsp+32], 0");
        em_line(cg, "    call WriteFile");
        break;
    }

    case NODE_SYS_BLOCK:
        gen_block(cg, n->body);
        break;

    case NODE_FN_CALL:
    case NODE_BINARY_OP:
        gen_expr(cg, idx);
        break;

    default:
        break;
    }
}

/* ============================================================
 *  §9  Top-level declarations
 * ============================================================ */
static void gen_blueprint(CodeGen *cg, ASTNode *n) {
    if (cg->bp_count >= 32) return;
    Blueprint *bp = &cg->bps[cg->bp_count++];
    my_strncpy(bp->name, n->sval, 63);
    bp->field_count = 0;
    for (int i = 0; i < n->child_count; i++) {
        ASTNode *f = &rdx_nodes[n->children[i]];
        my_strncpy(bp->field_names[bp->field_count], f->sval, 63);
        my_strncpy(bp->field_types[bp->field_count], f->rdx_type, 63);
        bp->field_count++;
    }
}

static void gen_fn(CodeGen *cg, ASTNode *n) {
    sym_clear(cg);

    /* First pass: count locals in body to estimate frame size */
    /* V1 simplification: use a generous 128 + 8*params */
    int est_locals = (n->child_count + 16) * 8;
    int fsize = calc_frame(est_locals);

    em(cg, "rdx_fn_"); em(cg, n->sval); em_line(cg, ":");
    em_line(cg, "    push rbp");
    em_line(cg, "    mov  rbp, rsp");
    em(cg, "    sub  rsp, "); em_num(cg, fsize); em_line(cg, "");

    /* Register parameters: rcx, rdx, r8, r9 */
    static const char *pregs[] = {"rcx","rdx","r8","r9"};
    for (int i = 0; i < n->child_count && i < 4; i++) {
        ASTNode *param = &rdx_nodes[n->children[i]];
        int off = sym_add(cg, param->sval, param->rdx_type);
        em(cg, "    mov  qword [rbp"); em_num(cg, off); em(cg, "], ");
        em_line(cg, pregs[i]);
    }

    gen_block(cg, n->body);

    em_line(cg, ".fn_ret:");
    em(cg, "    add  rsp, "); em_num(cg, fsize); em_line(cg, "");
    em_line(cg, "    pop  rbp");
    em_line(cg, "    ret");
    em_line(cg, "");
}

/* ============================================================
 *  §10  Public API
 * ============================================================ */
void codegen_init(CodeGen *cg) {
    my_memset(cg, 0, sizeof(CodeGen));
    cg->out.buf = (char*)rdx_alloc_bump(MAX_EMIT_SIZE);
    cg->out.cap = MAX_EMIT_SIZE;
    if (!cg->out.buf) rdx_panic("codegen_init: OOM");
}

int codegen_run(CodeGen *cg, int root) {
    ASTNode *prog = &rdx_nodes[root];

    /* ---- Collect blueprints first (need sizes for alloc) ---- */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_BLUEPRINT) gen_blueprint(cg, n);
    }

    /* ---- Pre-scan string literals for .data section ----------
       We do a two-pass approach: first collect, then emit header. */
    /* Temporary: emit into a side buffer, then prepend the .data section */
    /* body_cg is ~265 KB — must be heap-allocated, not stack */
    CodeGen *body_cg = (CodeGen*)rdx_alloc_bump(sizeof(CodeGen));
    if (!body_cg) rdx_panic("codegen_run: OOM for body_cg");
    my_memset(body_cg, 0, sizeof(CodeGen));
    body_cg->out.buf = (char*)rdx_alloc_bump(MAX_EMIT_SIZE / 2);
    body_cg->out.cap = MAX_EMIT_SIZE / 2;
    body_cg->bp_count = cg->bp_count;
    my_memcpy(body_cg->bps, cg->bps, sizeof(Blueprint) * cg->bp_count);

    /* Generate .text body into body_cg */
    /* sys.rdx block (entry) and functions */
    /* --- Global variables at top level emitted into _rdx_init --- */
    em_line(body_cg, "_rdx_init:");
    em_line(body_cg, "    push rbp");
    em_line(body_cg, "    mov  rbp, rsp");
    em_line(body_cg, "    sub  rsp, 64");
    sym_clear(body_cg);
    body_cg->frame_size = 0;

    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type != NODE_FN_DECL && n->type != NODE_BLUEPRINT)
            gen_stmt(body_cg, prog->children[i]);
    }
    em_line(body_cg, "    add  rsp, 64");
    em_line(body_cg, "    pop  rbp");
    em_line(body_cg, "    ret");
    em_line(body_cg, "");

    /* Function declarations */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_FN_DECL) gen_fn(body_cg, n);
    }

    /* Copy string pool from body_cg back to cg */
    cg->str_count = body_cg->str_count;
    my_memcpy(cg->strs, body_cg->strs, sizeof(cg->strs));

    /* ---- Now emit the full NASM file into cg->out ---- */

    /* File header */
    em_line(cg, "; Generated by RDX Compiler — Win64 NASM");
    em_line(cg, "bits 64");
    em_line(cg, "default rel");
    em_line(cg, "");
    em_line(cg, "extern GetStdHandle");
    em_line(cg, "extern WriteFile");
    em_line(cg, "extern VirtualAlloc");
    em_line(cg, "extern VirtualFree");
    em_line(cg, "extern ExitProcess");
    em_line(cg, "");

    /* .data section — string literals */
    em_line(cg, "section .data");
    for (int i = 0; i < cg->str_count; i++) {
        em(cg, "    _str"); em_num(cg, i); em(cg, "  db  ");
        /* Emit each byte of the string as decimal values */
        const char *s = cg->strs[i];
        int first = 1;
        while (*s) {
            if (!first) em(cg, ", ");
            em_num(cg, (unsigned char)*s++);
            first = 0;
        }
        /* Append CR+LF and NUL for console output */
        if (!first) em(cg, ", ");
        em_line(cg, "13, 10, 0");
        /* _strN_len  equ  $ - _strN */
        em(cg, "    _str"); em_num(cg, i);
        em(cg, "_len  equ  $ - _str"); em_num(cg, i); em(cg, "\n");
    }
    em_line(cg, "");

    /* .bss */
    em_line(cg, "section .bss");
    em_line(cg, "    _rdx_stdout  resq 1");
    em_line(cg, "    _rdx_written resd 1");
    em_line(cg, "");

    /* .text */
    em_line(cg, "section .text");
    em_line(cg, "global mainCRTStartup");
    em_line(cg, "");

    /* Entry point */
    em_line(cg, "mainCRTStartup:");
    em_line(cg, "    sub  rsp, 56       ; align + shadow space");
    em_line(cg, "    mov  rcx, -11      ; STD_OUTPUT_HANDLE");
    em_line(cg, "    call GetStdHandle");
    em_line(cg, "    mov  [rel _rdx_stdout], rax");
    em_line(cg, "    call _rdx_init");
    em_line(cg, "    xor  rcx, rcx");
    em_line(cg, "    call ExitProcess");
    em_line(cg, "");

    /* Append the body we generated */
    my_memcpy(cg->out.buf + cg->out.pos, body_cg->out.buf, body_cg->out.pos);
    cg->out.pos += body_cg->out.pos;

    cg->out.buf[cg->out.pos] = '\0';
    return cg->had_error ? -1 : 0;
}
