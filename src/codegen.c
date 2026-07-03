/* codegen.c — RDX Direct Machine-Code JIT Engine
   Evaluates the simple AST and emits x86-64 machine code 
   directly into memory for execution. 
   Deps: codegen.h -> parser.h -> lexer.h -> core.h */
#include "codegen.h"

// Removed MEM_COMMIT, MEM_RESERVE, MEM_RELEASE, STD_OUTPUT_HANDLE as they are in core.h
#define PAGE_EXECUTE_READWRITE 0x40
void* __stdcall VirtualAlloc(void* lpAddress, usize dwSize, unsigned int flAllocationType, unsigned int flProtect);
int __stdcall VirtualFree(void* lpAddress, usize dwSize, unsigned int dwFreeType);
void* __stdcall GetStdHandle(unsigned int nStdHandle);
int __stdcall WriteFile(void* hFile, const void* lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int* lpNumberOfBytesWritten, void* lpOverlapped);

/* ============================================================
 *  §1  Symbol Table
 * ============================================================ */
static JitVar *jit_get_var(JitEngine *jit, const char *name) {
    for (int i = 0; i < jit->var_count; i++) {
        if (my_strcmp(jit->vars[i].name, name) == 0) {
            return &jit->vars[i];
        }
    }
    return NULL;
}

static JitVar *jit_add_var(JitEngine *jit, const char *name, const char *type) {
    JitVar *v = jit_get_var(jit, name);
    if (!v) {
        if (jit->var_count >= 128) {
            jit->had_error = 1;
            return NULL;
        }
        v = &jit->vars[jit->var_count++];
        my_strncpy(v->name, name, 63);
    }
    my_strncpy(v->type, type, 63);
    v->ival = 0;
    v->sval[0] = '\0';
    return v;
}

/* ============================================================
 *  §2  String Building Utilities
 * ============================================================ */
static void int_to_str(i64 val, char* buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char temp[32];
    int i = 0;
    int is_neg = 0;
    if (val < 0) { is_neg = 1; val = -val; }
    while (val > 0) {
        temp[i++] = (val % 10) + '0';
        val /= 10;
    }
    int j = 0;
    if (is_neg) buf[j++] = '-';
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

static double str_to_flt(const char *s) {
    double res = 0.0, fact = 1.0;
    int point_seen = 0, sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s) {
        if (*s == '.') { point_seen = 1; }
        else if (*s >= '0' && *s <= '9') {
            if (point_seen) fact /= 10.0;
            res = res * 10.0 + (*s - '0');
        }
        s++;
    }
    return res * fact * sign;
}

static void flt_to_str(double val, char* buf) {
    if (val < 0) { *buf++ = '-'; val = -val; }
    i64 int_part = (i64)val;
    double frac_part = val - (double)int_part;
    
    char temp[32];
    int_to_str(int_part, temp);
    
    int i = 0;
    while (temp[i]) { *buf++ = temp[i++]; }
    
    *buf++ = '.';
    
    for (int j = 0; j < 6; j++) {
        frac_part *= 10.0;
        int d = (int)frac_part;
        *buf++ = d + '0';
        frac_part -= d;
    }
    
    while (*(buf - 1) == '0' && *(buf - 2) != '.') buf--;
    *buf = '\0';
}

/* ============================================================
 *  §3  AST Evaluation & Execution
 * ============================================================ */

/* Evaluates an expression node into a variable structure (as a temporary) */
static void jit_eval_expr(JitEngine *jit, int idx, JitVar *out_val) {
    if (idx < 0) return;
    ASTNode *n = &rdx_nodes[idx];
    
    if (n->type == NODE_LITERAL_NUM) {
        out_val->ival = n->ival;
        out_val->dval = (double)n->ival;
        int_to_str(n->ival, out_val->sval);
        my_strcpy(out_val->type, "num");
    } 
    else if (n->type == NODE_LITERAL_STR) {
        out_val->ival = 0;
        out_val->dval = 0.0;
        my_strcpy(out_val->sval, n->sval);
        my_strcpy(out_val->type, "chr");
    }
    else if (n->type == NODE_LITERAL_FLT) {
        out_val->dval = str_to_flt(n->sval);
        out_val->ival = (i64)out_val->dval;
        my_strcpy(out_val->sval, n->sval);
        my_strcpy(out_val->type, "flt");
    }
    else if (n->type == NODE_IDENT) {
        JitVar *v = jit_get_var(jit, n->sval);
        if (v) {
            out_val->ival = v->ival;
            out_val->dval = v->dval;
            my_strcpy(out_val->sval, v->sval);
            my_strcpy(out_val->type, v->type);
        } else {
            /* Undefined */
            rdx_print_err("[JIT ERROR] Undefined variable: ");
            rdx_print_err(n->sval);
            rdx_print_err("\r\n");
            jit->had_error = 1;
            out_val->ival = 0;
            out_val->dval = 0.0;
            out_val->sval[0] = '\0';
            my_strcpy(out_val->type, "unk");
        }
    }
    else if (n->type == NODE_BINARY_OP) {
        JitVar left_val, right_val;
        jit_eval_expr(jit, n->left, &left_val);
        jit_eval_expr(jit, n->right, &right_val);
        
        int is_flt = (my_strcmp(left_val.type, "flt") == 0 || my_strcmp(right_val.type, "flt") == 0 ||
                      my_strcmp(left_val.type, "dob") == 0 || my_strcmp(right_val.type, "dob") == 0);
        
        out_val->ival = 0;
        out_val->dval = 0.0;
        my_strcpy(out_val->type, is_flt ? "flt" : "num");
        
        if (is_flt) {
            if (my_strcmp(n->op, "+") == 0) out_val->dval = left_val.dval + right_val.dval;
            else if (my_strcmp(n->op, "-") == 0) out_val->dval = left_val.dval - right_val.dval;
            else if (my_strcmp(n->op, "*") == 0) out_val->dval = left_val.dval * right_val.dval;
            else if (my_strcmp(n->op, "/") == 0) out_val->dval = (right_val.dval != 0.0) ? (left_val.dval / right_val.dval) : 0.0;
            else if (my_strcmp(n->op, "==") == 0) out_val->ival = (left_val.dval == right_val.dval);
            else if (my_strcmp(n->op, "!=") == 0) out_val->ival = (left_val.dval != right_val.dval);
            else if (my_strcmp(n->op, "<") == 0) out_val->ival = (left_val.dval < right_val.dval);
            else if (my_strcmp(n->op, ">") == 0) out_val->ival = (left_val.dval > right_val.dval);
            else if (my_strcmp(n->op, "<=") == 0) out_val->ival = (left_val.dval <= right_val.dval);
            else if (my_strcmp(n->op, ">=") == 0) out_val->ival = (left_val.dval >= right_val.dval);
            
            if (my_strcmp(n->op, "+") == 0 || my_strcmp(n->op, "-") == 0 || my_strcmp(n->op, "*") == 0 || my_strcmp(n->op, "/") == 0) {
                out_val->ival = (i64)out_val->dval;
                flt_to_str(out_val->dval, out_val->sval);
            } else {
                my_strcpy(out_val->type, "blo");
                out_val->dval = (double)out_val->ival;
                int_to_str(out_val->ival, out_val->sval);
            }
        } else {
            if (my_strcmp(n->op, "+") == 0) out_val->ival = left_val.ival + right_val.ival;
            else if (my_strcmp(n->op, "-") == 0) out_val->ival = left_val.ival - right_val.ival;
            else if (my_strcmp(n->op, "*") == 0) out_val->ival = left_val.ival * right_val.ival;
            else if (my_strcmp(n->op, "/") == 0) out_val->ival = (right_val.ival != 0) ? (left_val.ival / right_val.ival) : 0;
            else if (my_strcmp(n->op, "==") == 0) { out_val->ival = (left_val.ival == right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, "!=") == 0) { out_val->ival = (left_val.ival != right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, "<") == 0) { out_val->ival = (left_val.ival < right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, ">") == 0) { out_val->ival = (left_val.ival > right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, "<=") == 0) { out_val->ival = (left_val.ival <= right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, ">=") == 0) { out_val->ival = (left_val.ival >= right_val.ival); my_strcpy(out_val->type, "blo"); }
            
            out_val->dval = (double)out_val->ival;
            int_to_str(out_val->ival, out_val->sval);
        }
    }
    else {
        out_val->ival = 0;
        out_val->dval = 0.0;
        out_val->sval[0] = '\0';
        my_strcpy(out_val->type, "unk");
    }
}

static void jit_emit_and_run(const char* final_string) {
    int str_len = my_strlen(final_string);
    unsigned char* jit_mem = (unsigned char*)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!jit_mem) {
        rdx_print_err("[JIT] VirtualAlloc failed.\r\n");
        return;
    }
    
    int pos = 0;
    
    // sub rsp, 40h
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x83; jit_mem[pos++] = 0xEC; jit_mem[pos++] = 0x40;
    
    // mov ecx, -11
    jit_mem[pos++] = 0xB9; jit_mem[pos++] = 0xF5; jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xFF;
    
    // mov rax, GetStdHandle
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xB8;
    void* pGetStdHandle = (void*)GetStdHandle;
    for (int i=0; i<8; i++) { jit_mem[pos++] = ((unsigned char*)&pGetStdHandle)[i]; }
    
    // call rax
    jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xD0;
    
    // mov rcx, rax
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x89; jit_mem[pos++] = 0xC1;
    
    // lea rdx, [rip + 37]
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x8D; jit_mem[pos++] = 0x15;
    jit_mem[pos++] = 37; jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0;
    
    // mov r8d, string_len
    jit_mem[pos++] = 0x41; jit_mem[pos++] = 0xB8;
    for (int i=0; i<4; i++) { jit_mem[pos++] = ((unsigned char*)&str_len)[i]; }
    
    // lea r9, [rsp + 30h]
    jit_mem[pos++] = 0x4C; jit_mem[pos++] = 0x8D; jit_mem[pos++] = 0x4C; jit_mem[pos++] = 0x24; jit_mem[pos++] = 0x30;
    
    // mov qword ptr [rsp + 20h], 0
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xC7; jit_mem[pos++] = 0x44; jit_mem[pos++] = 0x24; jit_mem[pos++] = 0x20;
    jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0; jit_mem[pos++] = 0;
    
    // mov rax, WriteFile
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0xB8;
    void* pWriteFile = (void*)WriteFile;
    for (int i=0; i<8; i++) { jit_mem[pos++] = ((unsigned char*)&pWriteFile)[i]; }
    
    // call rax
    jit_mem[pos++] = 0xFF; jit_mem[pos++] = 0xD0;
    
    // add rsp, 40h
    jit_mem[pos++] = 0x48; jit_mem[pos++] = 0x83; jit_mem[pos++] = 0xC4; jit_mem[pos++] = 0x40;
    
    // ret
    jit_mem[pos++] = 0xC3;
    
    // Append string
    for (int i=0; i<str_len; i++) {
        jit_mem[pos++] = final_string[i];
    }
    
    // Execute JIT buffer
    void (*run_rdx)(void) = (void (*)(void))jit_mem;
    run_rdx();
    
    VirtualFree(jit_mem, 0, MEM_RELEASE);
}

static void jit_exec_stmt(JitEngine *jit, int idx) {
    if (idx < 0) return;
    ASTNode *n = &rdx_nodes[idx];
    
    if (n->type == NODE_VAR_DECL) {
        JitVar *v = jit_add_var(jit, n->sval, n->rdx_type);
        if (v) {
            JitVar temp;
            jit_eval_expr(jit, n->left, &temp);
            v->ival = temp.ival;
            v->dval = temp.dval;
            my_strcpy(v->sval, temp.sval);
        }
    }
    else if (n->type == NODE_ASSIGN) {
        JitVar *v = jit_get_var(jit, n->sval);
        if (!v) {
            v = jit_add_var(jit, n->sval, "unk");
        }
        if (v) {
            JitVar temp;
            jit_eval_expr(jit, n->left, &temp);
            v->ival = temp.ival;
            v->dval = temp.dval;
            my_strcpy(v->sval, temp.sval);
            my_strcpy(v->type, temp.type);
        }
    }
    else if (n->type == NODE_SYS_CALL) {
        char out_buf[1024];
        out_buf[0] = '\0';
        
        /* 1st arg: base string */
        my_strcat(out_buf, n->sval);
        
        /* 2nd arg: array of expressions */
        if (n->left >= 0) {
            ASTNode *arr = &rdx_nodes[n->left];
            for (int i = 0; i < arr->child_count; i++) {
                JitVar temp;
                jit_eval_expr(jit, arr->children[i], &temp);
                if (jit->had_error) return;
                my_strcat(out_buf, temp.sval);
            }
        }
        
        my_strcat(out_buf, "\n");
        
        /* Emit and Run */
        jit_emit_and_run(out_buf);
    }
    else if (n->type == NODE_SYS_BLOCK || n->type == NODE_BLOCK) {
        int body = (n->type == NODE_SYS_BLOCK) ? n->body : idx;
        ASTNode *blk = &rdx_nodes[body];
        for (int i = 0; i < blk->child_count; i++) {
            if (jit->had_error) break;
            jit_exec_stmt(jit, blk->children[i]);
        }
    }
    else if (n->type == NODE_IF) {
        JitVar cond;
        jit_eval_expr(jit, n->condition, &cond);
        if (jit->had_error) return;
        
        if (cond.ival) {
            jit_exec_stmt(jit, n->body);
        } else if (n->else_body >= 0) {
            jit_exec_stmt(jit, n->else_body);
        }
    }
    else if (n->type == NODE_LOOP) {
        while (1) {
            JitVar cond;
            jit_eval_expr(jit, n->condition, &cond);
            if (jit->had_error || cond.ival == 0) break;
            jit_exec_stmt(jit, n->body);
        }
    }
}

/* ============================================================
 *  §4  Public API
 * ============================================================ */
void jit_init(JitEngine *jit) {
    my_memset(jit, 0, sizeof(JitEngine));
    JitVar *t = jit_add_var(jit, "TRUE", "blo");
    t->ival = 1;
    my_strcpy(t->sval, "1");
    
    JitVar *f = jit_add_var(jit, "FALSE", "blo");
    f->ival = 0;
    my_strcpy(f->sval, "0");
}

int jit_run(JitEngine *jit, int root) {
    ASTNode *prog = &rdx_nodes[root];
    
    for (int i = 0; i < prog->child_count; i++) {
        jit_exec_stmt(jit, prog->children[i]);
    }
    
    return jit->had_error ? -1 : 0;
}
