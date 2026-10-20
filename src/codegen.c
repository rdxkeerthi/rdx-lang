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
int __stdcall ReadConsoleA(void* hConsoleInput, void* lpBuffer, unsigned int nNumberOfCharsToRead, unsigned int* lpNumberOfCharsRead, void* pInputControl);
int __stdcall SetConsoleMode(void* hConsoleHandle, unsigned int dwMode);

/* forward declarations */
static void jit_emit_and_run(const char *final_string);
static void jit_exec_stmt(JitEngine *jit, int idx);

/* ============================================================
 *  §1  Symbol Table
 * ============================================================ */
static JitVar *jit_get_var(JitEngine *jit, const char *name) {
    /* Check current call-frame locals first (shadows global scope) */
    if (jit->frame_depth > 0) {
        JitFrame *fr = &jit->call_stack[jit->frame_depth - 1];
        for (int i = 0; i < fr->local_count; i++) {
            if (my_strcmp(fr->locals[i].name, name) == 0)
                return &fr->locals[i];
        }
    }
    /* Fall back to global variable table */
    for (int i = 0; i < jit->var_count; i++) {
        if (my_strcmp(jit->vars[i].name, name) == 0)
            return &jit->vars[i];
    }
    return NULL;
}

static JitVar *jit_add_var(JitEngine *jit, const char *name, const char *type) {
    JitVar *v = jit_get_var(jit, name);
    if (!v) {
        /* New variable goes into current call frame or global table */
        if (jit->frame_depth > 0) {
            JitFrame *fr = &jit->call_stack[jit->frame_depth - 1];
            if (fr->local_count >= JIT_MAX_LOCALS) { jit->had_error = 1; return NULL; }
            v = &fr->locals[fr->local_count++];
        } else {
            if (jit->var_count >= JIT_MAX_VARS) { jit->had_error = 1; return NULL; }
            v = &jit->vars[jit->var_count++];
        }
        my_strncpy(v->name, name, 63);
    }
    my_strncpy(v->type, type, 63);
    v->ival    = 0;
    v->dval    = 0.0;
    v->inst_id = -1;
    v->arr_id  = -1;
    v->is_ok   = 0; v->is_err  = 0;
    v->is_some = 0; v->is_none = 0;
    v->sval[0] = '\0';
    return v;
}

static void jit_err(JitEngine *jit, ASTNode *n, const char *msg, const char *arg) {
    if (n) rdx_diag_errorf(n->line, n->col, 1, "[JIT ERROR]", msg, arg);
    else rdx_diag_errorf(0, 0, 0, "[JIT ERROR]", msg, arg);
    if (jit) jit->had_error = 1;
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

/* Write a string directly (no JIT trampoline) — used for prompts */
static void direct_write(const char *s) {
    if (!s || !s[0]) return;
    rdx_print(s);
}

/* Read one line of user input from stdin into buf (up to max_chars).
   Also prints the prompt string first.
   Returns number of chars read (excluding newline). */
static int jit_read_line(const char *prompt, char *buf, int max_chars) {
    /* Print prompt */
    if (prompt && prompt[0]) direct_write(prompt);
    return rdx_read_line(buf, max_chars);
}

/* Convert string to i64 (simple atoi) */
static i64 jit_str_to_i64(const char *s) {
    i64 v = 0; int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/* Evaluates an expression node into a variable structure (as a temporary) */
static void jit_eval_expr(JitEngine *jit, int idx, JitVar *out_val) {
    if (idx < 0 || jit->had_error || jit->return_flag) return;
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
            out_val->ival    = v->ival;
            out_val->dval    = v->dval;
            out_val->inst_id = v->inst_id;
            out_val->arr_id  = v->arr_id;
            out_val->is_ok   = v->is_ok;
            out_val->is_err  = v->is_err;
            out_val->is_some = v->is_some;
            out_val->is_none = v->is_none;
            my_strcpy(out_val->sval, v->sval);
            my_strcpy(out_val->type, v->type);
        } else {
            jit_err(jit, n, "Undefined variable: ", n->sval);
            out_val->ival = 0; out_val->dval = 0.0;
            out_val->sval[0] = '\0'; my_strcpy(out_val->type, "unk");
        }
    }
    else if (n->type == NODE_GET) {
        /* get("prompt") — read a line from stdin */
        char input_buf[512];
        input_buf[0] = '\0';
        jit_read_line(n->sval, input_buf, 512);

        my_strcpy(out_val->sval, input_buf);
        my_strcpy(out_val->type, "chr");

        /* Auto-detect numeric: if all digits (optionally leading minus), set ival too */
        const char *p = input_buf;
        int is_num = 1;
        if (*p == '-') p++;
        if (!*p) is_num = 0;
        for (const char *q = p; *q; q++)
            if (*q < '0' || *q > '9') { is_num = 0; break; }
        if (is_num) {
            out_val->ival = jit_str_to_i64(input_buf);
            out_val->dval = (double)out_val->ival;
            my_strcpy(out_val->type, "num");   /* upgrade to num if pure integer */
        } else {
            out_val->ival = 0;
            out_val->dval = 0.0;
        }
    }
    else if (n->type == NODE_BINARY_OP) {
        /* unary not / neg — right is -1 */
        if (n->right < 0) {
            JitVar operand;
            jit_eval_expr(jit, n->left, &operand);
            if (my_strcmp(n->op, "not") == 0) {
                out_val->ival = !operand.ival;
                out_val->dval = 0.0;
                my_strcpy(out_val->type, "blo");
                int_to_str(out_val->ival, out_val->sval);
            } else { /* neg */
                out_val->ival = -operand.ival;
                out_val->dval = -operand.dval;
                my_strcpy(out_val->type, operand.type);
                int_to_str(out_val->ival, out_val->sval);
            }
            return;
        }
        JitVar left_val, right_val;
        jit_eval_expr(jit, n->left, &left_val);
        /* short-circuit: and/or */
        if (my_strcmp(n->op, "and") == 0) {
            if (!left_val.ival) { out_val->ival=0; out_val->dval=0.0; my_strcpy(out_val->type,"blo"); my_strcpy(out_val->sval,"0"); return; }
            jit_eval_expr(jit, n->right, &right_val);
            out_val->ival = right_val.ival ? 1 : 0;
            out_val->dval = 0.0; my_strcpy(out_val->type,"blo"); int_to_str(out_val->ival,out_val->sval); return;
        }
        if (my_strcmp(n->op, "or") == 0) {
            if (left_val.ival)  { out_val->ival=1; out_val->dval=0.0; my_strcpy(out_val->type,"blo"); my_strcpy(out_val->sval,"1"); return; }
            jit_eval_expr(jit, n->right, &right_val);
            out_val->ival = right_val.ival ? 1 : 0;
            out_val->dval = 0.0; my_strcpy(out_val->type,"blo"); int_to_str(out_val->ival,out_val->sval); return;
        }
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
            else if (my_strcmp(n->op, "==") == 0) { 
                if (my_strcmp(left_val.type, "chr") == 0 || my_strcmp(right_val.type, "chr") == 0) out_val->ival = (my_strcmp(left_val.sval, right_val.sval) == 0); 
                else out_val->ival = (left_val.ival == right_val.ival); 
                my_strcpy(out_val->type, "blo"); 
            }
            else if (my_strcmp(n->op, "!=") == 0) { 
                if (my_strcmp(left_val.type, "chr") == 0 || my_strcmp(right_val.type, "chr") == 0) out_val->ival = (my_strcmp(left_val.sval, right_val.sval) != 0); 
                else out_val->ival = (left_val.ival != right_val.ival); 
                my_strcpy(out_val->type, "blo"); 
            }
            else if (my_strcmp(n->op, "<") == 0) { out_val->ival = (left_val.ival < right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, ">") == 0) { out_val->ival = (left_val.ival > right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, "<=") == 0) { out_val->ival = (left_val.ival <= right_val.ival); my_strcpy(out_val->type, "blo"); }
            else if (my_strcmp(n->op, ">=") == 0) { out_val->ival = (left_val.ival >= right_val.ival); my_strcpy(out_val->type, "blo"); }
            
            out_val->dval = (double)out_val->ival;
            int_to_str(out_val->ival, out_val->sval);
        }
    }
    /* string + string concatenation */
    else if (n->type == NODE_BINARY_OP &&
             my_strcmp(n->op, "+") == 0) {
        /* handled below in binary op — but catch chr+chr explicitly */
        JitVar lv, rv;
        jit_eval_expr(jit, n->left,  &lv);
        jit_eval_expr(jit, n->right, &rv);
        if (my_strcmp(lv.type, "chr") == 0 || my_strcmp(rv.type, "chr") == 0) {
            my_strcpy(out_val->sval, lv.sval);
            my_strcat(out_val->sval, rv.sval);
            my_strcpy(out_val->type, "chr");
            out_val->ival = 0; out_val->dval = 0.0;
            return;
        }
        /* fall through to regular num math below — re-evaluate */
        out_val->ival  = lv.ival + rv.ival;
        out_val->dval  = lv.dval + rv.dval;
        my_strcpy(out_val->type, "num");
        int_to_str(out_val->ival, out_val->sval);
        return;
    }
    /* built-in function calls */
    else if (n->type == NODE_FN_CALL) {
        const char *fn = n->sval;
        /* collect args */
        JitVar args[8]; int argc = 0;
        for (int i = 0; i < n->child_count && i < 8; i++) {
            my_memset(&args[argc], 0, sizeof(JitVar));
            args[argc].inst_id = -1; args[argc].arr_id = -1;
            jit_eval_expr(jit, n->children[i], &args[argc]);
            argc++;
        }
        out_val->ival = 0; out_val->dval = 0.0; out_val->sval[0] = '\0';
        my_strcpy(out_val->type, "num");

        /* ─── impl method dispatch: obj.method(args) ───────────────────
         * The parser stored the call name as "obj.method" in n->sval.
         * We look up 'obj', find its shape type, build "Shape::method",
         * and dispatch exactly like a regular user-defined function but
         * with the receiver JitVar injected as the 'self' local.
         * ──────────────────────────────────────────────────────────────*/
        {
            /* Find the dot, if any */
            int dot_pos = -1;
            for (int k = 0; fn[k]; k++) { if (fn[k] == '.') { dot_pos = k; break; } }
            if (dot_pos > 0) {
                /* split "obj" and "method" */
                char obj_name[256]; char meth_name[128];
                my_strncpy(obj_name, fn, dot_pos); obj_name[dot_pos] = '\0';
                my_strcpy(meth_name, fn + dot_pos + 1);

                JitVar *receiver = jit_get_var(jit, obj_name);
                if (!receiver || receiver->inst_id < 0) {
                    jit_err(jit, n, "method call on non-struct variable: ", obj_name);
                    jit->had_error = 1; return;
                }
                JitInstance *recv_inst = &jit->instances[receiver->inst_id];

                /* Build "ShapeName::method" lookup key */
                char method_key[256];
                my_strcpy(method_key, recv_inst->shape_name);
                my_strcat(method_key, "::");
                my_strcat(method_key, meth_name);

                /* Find the method in the function table */
                int mfound = 0;
                for (int fi = 0; fi < jit->fn_count; fi++) {
                    if (my_strcmp(jit->fns[fi].name, method_key) == 0) {
                        mfound = 1;
                        if (jit->frame_depth >= JIT_MAX_FRAMES) {
                            jit_err(jit, n, "Stack overflow in method call", NULL);
                            jit->had_error = 1; return;
                        }
                        /* Push fresh activation frame */
                        JitFrame *fr = &jit->call_stack[jit->frame_depth];
                        my_memset(fr, 0, sizeof(JitFrame));
                        jit->frame_depth++;

                        /* Bind 'self' local = the receiver JitVar */
                        if (fr->local_count < JIT_MAX_LOCALS) {
                            JitVar *sv = &fr->locals[fr->local_count++];
                            my_strcpy(sv->name, "self");
                            sv->inst_id = receiver->inst_id;
                            sv->arr_id  = receiver->arr_id;
                            sv->ival    = receiver->ival;
                            sv->dval    = receiver->dval;
                            my_strcpy(sv->sval, receiver->sval);
                            my_strcpy(sv->type, receiver->type);
                        }

                        /* Bind remaining args to parameters (skip self param) */
                        ASTNode *fn_node = &rdx_nodes[jit->fns[fi].node_idx];
                        int param_start = 0;
                        if (fn_node->child_count > 0) {
                            ASTNode *p0 = &rdx_nodes[fn_node->children[0]];
                            if (my_strcmp(p0->sval, "self") == 0) param_start = 1;
                        }
                        for (int pi = param_start; pi < fn_node->child_count && (pi - param_start) < argc; pi++) {
                            ASTNode *param = &rdx_nodes[fn_node->children[pi]];
                            if (fr->local_count < JIT_MAX_LOCALS) {
                                JitVar *lv = &fr->locals[fr->local_count++];
                                my_strncpy(lv->name, param->sval,     63);
                                my_strncpy(lv->type, param->rdx_type, 63);
                                lv->ival = args[pi - param_start].ival;
                                lv->dval = args[pi - param_start].dval;
                                my_strcpy(lv->sval, args[pi - param_start].sval);
                            }
                        }

                        /* Execute method body */
                        int prev_ret = jit->return_flag;
                        jit->return_flag = 0;
                        my_memset(&jit->ret_val, 0, sizeof(JitVar));
                        jit_exec_stmt(jit, fn_node->body);

                        /* Copy return value */
                        out_val->ival = jit->ret_val.ival;
                        out_val->dval = jit->ret_val.dval;
                        my_strcpy(out_val->sval, jit->ret_val.sval);
                        my_strcpy(out_val->type, jit->ret_val.type);
                        if (out_val->type[0] == '\0') my_strcpy(out_val->type, "num");
                        jit->return_flag = jit->return_flag ? 1 : prev_ret;

                        /* Write back any mutated fields from 'self' local back
                         * to the instance so field changes persist.            */
                        JitFrame *done_fr = &jit->call_stack[jit->frame_depth - 1];
                        for (int li = 0; li < done_fr->local_count; li++) {
                            if (my_strcmp(done_fr->locals[li].name, "self") == 0) {
                                /* self is a handle — field mutations went directly
                                 * through inst_id, so no extra copy needed here. */
                                break;
                            }
                        }

                        jit->frame_depth--;
                        break;
                    }
                }
                if (!mfound) {
                    jit_err(jit, n, "unknown method: ", method_key);
                    jit->had_error = 1;
                }
                return;
            }
        }

        /* ─── output / newline ─── */
        if (my_strcmp(fn, "nl") == 0 || my_strcmp(fn, "newline") == 0) {
            jit_emit_and_run("\n");
            out_val->sval[0] = '\0'; out_val->ival = 0;
            return;
        }
        /* ─── math ─── */
        if (my_strcmp(fn, "abs") == 0 && argc >= 1) {
            out_val->ival = args[0].ival < 0 ? -args[0].ival : args[0].ival;
            out_val->dval = args[0].dval < 0.0 ? -args[0].dval : args[0].dval;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "max") == 0 && argc >= 2) {
            out_val->ival = args[0].ival > args[1].ival ? args[0].ival : args[1].ival;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "min") == 0 && argc >= 2) {
            out_val->ival = args[0].ival < args[1].ival ? args[0].ival : args[1].ival;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "pow") == 0 && argc >= 2) {
            i64 base = args[0].ival, exp2 = args[1].ival, r = 1;
            for (i64 k = 0; k < exp2; k++) r *= base;
            out_val->ival = r; int_to_str(r, out_val->sval);
        } else if (my_strcmp(fn, "clamp") == 0 && argc >= 3) {
            i64 v = args[0].ival, lo = args[1].ival, hi = args[2].ival;
            out_val->ival = v < lo ? lo : (v > hi ? hi : v);
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "sqrt") == 0 && argc >= 1) {
            /* Newton-Raphson integer sqrt */
            i64 n2 = args[0].ival < 0 ? 0 : args[0].ival;
            i64 x = n2, y = (n2 + 1) / 2;
            while (y < x) { x = y; y = (y + n2 / (y ? y : 1)) / 2; }
            out_val->ival = x; int_to_str(x, out_val->sval);
        } else if (my_strcmp(fn, "mod") == 0 && argc >= 2) {
            out_val->ival = args[1].ival ? (args[0].ival % args[1].ival) : 0;
            int_to_str(out_val->ival, out_val->sval);
        }
        /* ─── string ─── */
        else if (my_strcmp(fn, "len") == 0 && argc >= 1) {
            out_val->ival = (i64)my_strlen(args[0].sval);
            out_val->dval = (double)out_val->ival;
            int_to_str(out_val->ival, out_val->sval);
            my_strcpy(out_val->type, "num");
        } else if (my_strcmp(fn, "upper") == 0 && argc >= 1) {
            my_strcpy(out_val->sval, args[0].sval);
            for (int k = 0; out_val->sval[k]; k++)
                if (out_val->sval[k] >= 'a' && out_val->sval[k] <= 'z')
                    out_val->sval[k] -= 32;
            my_strcpy(out_val->type, "chr");
        } else if (my_strcmp(fn, "lower") == 0 && argc >= 1) {
            my_strcpy(out_val->sval, args[0].sval);
            for (int k = 0; out_val->sval[k]; k++)
                if (out_val->sval[k] >= 'A' && out_val->sval[k] <= 'Z')
                    out_val->sval[k] += 32;
            my_strcpy(out_val->type, "chr");
        } else if (my_strcmp(fn, "concat") == 0 && argc >= 2) {
            my_strcpy(out_val->sval, args[0].sval);
            my_strcat(out_val->sval, args[1].sval);
            my_strcpy(out_val->type, "chr");
        } else if (my_strcmp(fn, "num") == 0 && argc >= 1) {
            /* type cast: num(x) — convert to integer */
            out_val->ival = args[0].ival;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "str") == 0 && argc >= 1) {
            /* type cast: str(x) — convert to string */
            my_strcpy(out_val->sval, args[0].sval);
            my_strcpy(out_val->type, "chr");
        } else if (my_strcmp(fn, "even") == 0 && argc >= 1) {
            out_val->ival = (args[0].ival % 2 == 0) ? 1 : 0;
            my_strcpy(out_val->type, "blo");
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "odd") == 0 && argc >= 1) {
            out_val->ival = (args[0].ival % 2 != 0) ? 1 : 0;
            my_strcpy(out_val->type, "blo");
            int_to_str(out_val->ival, out_val->sval);

        /* ─── modulo ─── */
        } else if (my_strcmp(fn, "mod") == 0 && argc >= 2) {
            i64 divisor = args[1].ival;
            out_val->ival = (divisor != 0) ? (args[0].ival % divisor) : 0;
            int_to_str(out_val->ival, out_val->sval);

        /* ─── type casting ─── */
        } else if ((my_strcmp(fn, "str") == 0 || my_strcmp(fn, "tostr") == 0) && argc >= 1) {
            my_strcpy(out_val->sval, args[0].sval);
            my_strcpy(out_val->type, "chr");
            out_val->ival = 0;
        } else if ((my_strcmp(fn, "num") == 0 || my_strcmp(fn, "tonum") == 0) && argc >= 1) {
            out_val->ival = jit_str_to_i64(args[0].sval);
            out_val->dval = (double)out_val->ival;
            int_to_str(out_val->ival, out_val->sval);
            my_strcpy(out_val->type, "num");
        } else if ((my_strcmp(fn, "flt") == 0 || my_strcmp(fn, "toflt") == 0) && argc >= 1) {
            out_val->dval = str_to_flt(args[0].sval);
            out_val->ival = (i64)out_val->dval;
            flt_to_str(out_val->dval, out_val->sval);
            my_strcpy(out_val->type, "flt");

        /* ─── type query ─── */
        } else if (my_strcmp(fn, "type") == 0 && argc >= 1) {
            my_strcpy(out_val->sval, args[0].type);
            my_strcpy(out_val->type, "chr");
            out_val->ival = 0;


        /* ─── string utilities ─── */
        } else if (my_strcmp(fn, "chr_at") == 0 && argc >= 2) {
            /* chr_at(str, idx) -> single char string */
            int si = (int)args[1].ival;
            int slen = (int)my_strlen(args[0].sval);
            if (si >= 0 && si < slen) {
                out_val->sval[0] = args[0].sval[si];
                out_val->sval[1] = '\0';
            } else {
                jit_err(jit, n, "String index out of bounds", NULL);
                return;
            }
            my_strcpy(out_val->type, "chr");
            out_val->ival = 0;
        } else if (my_strcmp(fn, "repeat") == 0 && argc >= 2) {
            /* repeat(str, n) -> repeated string */
            out_val->sval[0] = '\0';
            i64 times = args[1].ival;
            for (i64 ri = 0; ri < times && my_strlen(out_val->sval) + my_strlen(args[0].sval) < 500; ri++)
                my_strcat(out_val->sval, args[0].sval);
            my_strcpy(out_val->type, "chr");
            out_val->ival = 0;
        } else if (my_strcmp(fn, "digits") == 0 && argc >= 1) {
            /* digits(n) -> number of decimal digits */
            i64 v = args[0].ival < 0 ? -args[0].ival : args[0].ival;
            if (v == 0) { out_val->ival = 1; }
            else { out_val->ival = 0; while (v > 0) { out_val->ival++; v /= 10; } }
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "sign") == 0 && argc >= 1) {
            out_val->ival = (args[0].ival > 0) ? 1 : (args[0].ival < 0) ? -1 : 0;
            int_to_str(out_val->ival, out_val->sval);

        /* ─── boolean cast ─── */
        } else if (my_strcmp(fn, "bool") == 0 && argc >= 1) {
            out_val->ival = (args[0].ival != 0) ? 1 : 0;
            my_strcpy(out_val->type, "blo");
            int_to_str(out_val->ival, out_val->sval);

        /* ─── pseudo-random (deterministic based on input) ─── */
        } else if (my_strcmp(fn, "randish") == 0 && argc >= 1) {
            /* Not truly random — gives a hash-like value for demos */
            i64 seed = args[0].ival;
            out_val->ival = ((seed * 1664525LL + 1013904223LL) >> 8) & 0x7FFFFFFF;
            int_to_str(out_val->ival, out_val->sval);

        /* ─── Step 4: Arena memory management ─── */
        } else if (my_strcmp(fn, "arena.create") == 0 && argc >= 1) {
            /* arena.create(size) — allocate an arena, return arena-id as ival */
            usize sz = (usize)(args[0].ival > 0 ? args[0].ival : 64);
            if (jit->arena_count >= JIT_MAX_ARENAS) {
                jit_err(jit, n, "Too many arenas", NULL); return;
            }
            JitArena *ar = &jit->arenas[jit->arena_count];
            ar->mem = rdx_va_alloc(sz);
            if (!ar->mem) { jit_err(jit, n, "alloc failed", NULL); return; }
            ar->capacity = sz; ar->used = 0; ar->in_use = 1;
            out_val->ival = jit->arena_count++;
            my_strcpy(out_val->type, "arena");
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "arena.destroy") == 0 && argc >= 1) {
            /* arena.destroy(arena_id) — free the arena */
            int aid = (int)args[0].ival;
            if (aid >= 0 && aid < jit->arena_count && jit->arenas[aid].in_use) {
                rdx_va_free(jit->arenas[aid].mem);
                jit->arenas[aid].in_use = 0;
                jit->arenas[aid].mem = NULL;
            }
            out_val->ival = 0; my_strcpy(out_val->type, "num");
        } else if (my_strcmp(fn, "arena.reset") == 0 && argc >= 1) {
            /* arena.reset(arena_id) — reset bump pointer */
            int aid = (int)args[0].ival;
            if (aid >= 0 && aid < jit->arena_count && jit->arenas[aid].in_use)
                jit->arenas[aid].used = 0;
            out_val->ival = 0; my_strcpy(out_val->type, "num");
        } else if (my_strcmp(fn, "arena.used") == 0 && argc >= 1) {
            /* arena.used(arena_id) — bytes used so far */
            int aid = (int)args[0].ival;
            if (aid >= 0 && aid < jit->arena_count && jit->arenas[aid].in_use)
                out_val->ival = (i64)jit->arenas[aid].used;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "arena.cap") == 0 && argc >= 1) {
            /* arena.cap(arena_id) — total capacity */
            int aid = (int)args[0].ival;
            if (aid >= 0 && aid < jit->arena_count && jit->arenas[aid].in_use)
                out_val->ival = (i64)jit->arenas[aid].capacity;
            int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "arena.alloc") == 0 && argc >= 2) {
            /* arena.alloc(arena_id, size) -> returns raw pointer as ival */
            int aid = (int)args[0].ival;
            usize sz = (usize)args[1].ival;
            if (aid >= 0 && aid < jit->arena_count && jit->arenas[aid].in_use) {
                JitArena *ar = &jit->arenas[aid];
                if (ar->used + sz <= ar->capacity) {
                    out_val->ival = (i64)((usize)ar->mem + ar->used);
                    ar->used += sz;
                } else {
                    jit_err(jit, n, "arena out of memory", NULL);
                    jit->had_error = 1;
                    out_val->ival = 0;
                }
            } else {
                out_val->ival = 0;
            }
            my_strcpy(out_val->type, "num");
            int_to_str(out_val->ival, out_val->sval);

        /* ─── Step 5: Result / option types ─── */
        } else if (my_strcmp(fn, "is_ok") == 0 && argc >= 1) {
            out_val->ival = args[0].is_ok ? 1 : 0;
            my_strcpy(out_val->type, "blo"); int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "is_err") == 0 && argc >= 1) {
            out_val->ival = args[0].is_err ? 1 : 0;
            my_strcpy(out_val->type, "blo"); int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "is_some") == 0 && argc >= 1) {
            out_val->ival = args[0].is_some ? 1 : 0;
            my_strcpy(out_val->type, "blo"); int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "is_none") == 0 && argc >= 1) {
            out_val->ival = args[0].is_none ? 1 : 0;
            my_strcpy(out_val->type, "blo"); int_to_str(out_val->ival, out_val->sval);
        } else if (my_strcmp(fn, "unwrap") == 0 && argc >= 1) {
            /* unwrap(result) — get inner value or trigger error */
            if (args[0].is_err || args[0].is_none) {
                jit_err(jit, n, "unwrap() called on Err/None", NULL);
                jit->had_error = 1; return;
            }
            out_val->ival = args[0].ival; out_val->dval = args[0].dval;
            my_strcpy(out_val->sval, args[0].sval);
            my_strcpy(out_val->type, args[0].type);
        } else if (my_strcmp(fn, "unwrap_or") == 0 && argc >= 2) {
            /* unwrap_or(result, default) */
            if (args[0].is_err || args[0].is_none) {
                out_val->ival = args[1].ival; out_val->dval = args[1].dval;
                my_strcpy(out_val->sval, args[1].sval);
                my_strcpy(out_val->type, args[1].type);
            } else {
                out_val->ival = args[0].ival; out_val->dval = args[0].dval;
                my_strcpy(out_val->sval, args[0].sval);
                my_strcpy(out_val->type, args[0].type);
            }

        /* ─── Step 8: C FFI — extern("funcname", args...) ─── */
        } else if (my_strcmp(fn, "extern") == 0 && argc >= 1) {
            /* First arg must be the C function name as a string */
            const char *cfn_name = args[0].sval;
            /* Look up via GetProcAddress in current process */
            void *hmod = NULL;  /* NULL = current process on Win32 GetModuleHandle */
            typedef void* (__stdcall *GetModH_t)(const char*);
            typedef void* (__stdcall *GetProcA_t)(void*, const char*);
            /* We already link kernel32 which has LoadLibraryA/GetProcAddress */
            extern void* __stdcall GetModuleHandleA(const char*);
            extern void* __stdcall GetProcAddress(void*, const char*);
            hmod = GetModuleHandleA(NULL);
            typedef i64 (*cfn_t)(i64, i64, i64, i64);
            cfn_t cfp = (cfn_t)GetProcAddress(hmod, cfn_name);
            if (!cfp) {
                /* Try msvcrt */
                void *hmsvcrt = GetModuleHandleA("msvcrt.dll");
                if (hmsvcrt) cfp = (cfn_t)GetProcAddress(hmsvcrt, cfn_name);
            }
            if (!cfp) {
                jit_err(jit, n, "extern: function not found: ", cfn_name);
                jit->had_error = 1; return;
            }
            /* Call with up to 3 extra args (argc-1 args after name) */
            i64 a1=0, a2=0, a3=0;
            if (argc > 1) a1 = args[1].ival ? args[1].ival : (i64)(void*)args[1].sval;
            if (argc > 2) a2 = args[2].ival ? args[2].ival : (i64)(void*)args[2].sval;
            if (argc > 3) a3 = args[3].ival ? args[3].ival : (i64)(void*)args[3].sval;
            out_val->ival = cfp(a1, a2, a3, 0);
            int_to_str(out_val->ival, out_val->sval);
            my_strcpy(out_val->type, "num");

        } else {
            /* Unknown built-in — check user-defined function table */
            int fn_found = 0;
            for (int fi = 0; fi < jit->fn_count; fi++) {
                if (my_strcmp(jit->fns[fi].name, fn) == 0) {
                    fn_found = 1;
                    if (jit->frame_depth >= JIT_MAX_FRAMES) {
                        jit_err(jit, n, "Stack overflow", NULL);
                        jit->had_error = 1;
                        return;
                    }
                    /* Push a fresh activation frame */
                    JitFrame *fr = &jit->call_stack[jit->frame_depth];
                    my_memset(fr, 0, sizeof(JitFrame));
                    jit->frame_depth++;
                    /* Bind parameters as locals in the new frame */
                    ASTNode *fn_node = &rdx_nodes[jit->fns[fi].node_idx];
                    for (int pi = 0; pi < fn_node->child_count && pi < argc; pi++) {
                        ASTNode *param = &rdx_nodes[fn_node->children[pi]];
                        if (fr->local_count < JIT_MAX_LOCALS) {
                            JitVar *lv = &fr->locals[fr->local_count++];
                            my_strncpy(lv->name, param->sval,     63);
                            my_strncpy(lv->type, param->rdx_type, 63);
                            lv->ival = args[pi].ival;
                            lv->dval = args[pi].dval;
                            my_strcpy(lv->sval, args[pi].sval);
                        }
                    }
                    /* Execute body, capturing return value */
                    int prev_ret = jit->return_flag;
                    jit->return_flag = 0;
                    my_memset(&jit->ret_val, 0, sizeof(JitVar));
                    jit_exec_stmt(jit, fn_node->body);
                    /* Copy return value field-by-field (no memcpy under -nostdlib) */
                    out_val->ival = jit->ret_val.ival;
                    out_val->dval = jit->ret_val.dval;
                    my_strcpy(out_val->sval, jit->ret_val.sval);
                    my_strcpy(out_val->type, jit->ret_val.type);
                    if (out_val->type[0] == '\0') my_strcpy(out_val->type, "num");
                    jit->return_flag = prev_ret;  /* restore caller's flag */
                    /* Pop the frame */
                    jit->frame_depth--;
                    break;
                }
            }
            if (!fn_found) {

                /* Check extern functions table */
                for (int fi = 0; fi < jit->extern_fn_count; fi++) {
                    if (my_strcmp(jit->extern_fns[fi].name, fn) == 0) {
                        fn_found = 1;
                        JitExternFn *efn = &jit->extern_fns[fi];
                        
                        /* Resolve address on first call */
                        if (!efn->fn_ptr) {
                            extern void* __stdcall GetModuleHandleA(const char*);
                            extern void* __stdcall LoadLibraryA(const char*);
                            extern void* __stdcall GetProcAddress(void*, const char*);
                            
                            void *hmod = NULL;
                            if (efn->lib_name[0] == '\0') {
                                hmod = GetModuleHandleA(NULL);
                            } else {
                                hmod = GetModuleHandleA(efn->lib_name);
                                if (!hmod) {
                                    hmod = LoadLibraryA(efn->lib_name);
                                    if (hmod && jit->module_count < 16) {
                                        jit->modules[jit->module_count++] = hmod;
                                    }
                                }
                            }
                            if (hmod) {
                                efn->fn_ptr = GetProcAddress(hmod, efn->name);
                            }
                            if (!efn->fn_ptr) {
                                jit_err(jit, n, "extern function not found: ", efn->name);
                                jit->had_error = 1; return;
                            }
                        }
                        
                        /* Call FFI (supporting up to 6 args on Win64 calling convention) */
                        typedef i64 (*cfn_t)(i64, i64, i64, i64, i64, i64);
                        cfn_t cfp = (cfn_t)efn->fn_ptr;
                        
                        i64 a[6] = {0};
                        for (int pi = 0; pi < argc && pi < 6; pi++) {
                            /* Treat strings as pointers */
                            if (my_strcmp(args[pi].type, "chr") == 0) {
                                a[pi] = (i64)(void*)args[pi].sval;
                            } else {
                                a[pi] = args[pi].ival;
                            }
                        }
                        
                        out_val->ival = cfp(a[0], a[1], a[2], a[3], a[4], a[5]);
                        out_val->dval = (double)out_val->ival;
                        int_to_str(out_val->ival, out_val->sval);
                        


                        ASTNode *enode = &rdx_nodes[efn->node_idx];
                        if (enode->rdx_type[0]) {
                            my_strcpy(out_val->type, enode->rdx_type);
                        } else {
                            my_strcpy(out_val->type, "num");
                        }
                        break;
                    }
                }
            }
            if (!fn_found) {
                out_val->ival = 0; out_val->sval[0] = '\0';
                my_strcpy(out_val->type, "num");
            }
        }
        return;
    }
    /* ── Step 5: Result constructors ── */
    else if (n->type == NODE_RESULT) {
        if (my_strcmp(n->op, "ok") == 0) {
            jit_eval_expr(jit, n->left, out_val);
            out_val->is_ok = 1; out_val->is_err = 0; out_val->is_some = 0; out_val->is_none = 0;
        } else if (my_strcmp(n->op, "err") == 0) {
            jit_eval_expr(jit, n->left, out_val);
            out_val->is_err = 1; out_val->is_ok = 0; out_val->is_some = 0; out_val->is_none = 0;
        } else if (my_strcmp(n->op, "some") == 0) {
            jit_eval_expr(jit, n->left, out_val);
            out_val->is_some = 1; out_val->is_none = 0; out_val->is_ok = 0; out_val->is_err = 0;
        } else if (my_strcmp(n->op, "none") == 0) {
            out_val->ival = 0; out_val->dval = 0.0;
            my_strcpy(out_val->sval, "none"); my_strcpy(out_val->type, "none");
            out_val->inst_id = -1; out_val->arr_id = -1;
            out_val->is_none = 1; out_val->is_some = 0; out_val->is_ok = 0; out_val->is_err = 0;
        }
    }
    /* ── Step 5: Error Propagation (?) ── */
    else if (n->type == NODE_PROPAGATE) {
        jit_eval_expr(jit, n->left, out_val);
        if (out_val->is_err || out_val->is_none) {
            /* Trigger early return with this value */
            jit->ret_val = *out_val;
            jit->return_flag = 1;
        } else {
            /* Strip the ok/some wrapper, return the inner value */
            out_val->is_ok = 0; out_val->is_some = 0;
        }
    }
    /* ── Step 2: Struct literal  ShapeName { field: val, ... } ── */
    else if (n->type == NODE_STRUCT_LIT) {
        /* Find or verify the shape type */
        int shape_idx = -1;
        for (int si = 0; si < jit->shape_count; si++) {
            if (my_strcmp(jit->shapes[si].shape_name, n->sval) == 0) { shape_idx = si; break; }
        }
        if (shape_idx < 0) {
            jit_err(jit, n, "Unknown shape: ", n->sval);
            jit->had_error = 1; return;
        }
        if (jit->instance_count >= JIT_MAX_INSTANCES) {
            jit_err(jit, n, "Too many struct instances", NULL); return;
        }
        JitInstance *inst = &jit->instances[jit->instance_count];
        my_memset(inst, 0, sizeof(JitInstance));
        inst->in_use = 1;
        my_strcpy(inst->shape_name, n->sval);
        JitShape *sh = &jit->shapes[shape_idx];
        inst->field_count = sh->field_count;
        /* Initialize all fields to zero */
        for (int fi = 0; fi < sh->field_count; fi++) {
            my_strcpy(inst->fields[fi].name, sh->field_names[fi]);
            my_strcpy(inst->fields[fi].type, sh->field_types[fi]);
            inst->fields[fi].ival = 0; inst->fields[fi].dval = 0.0;
            inst->fields[fi].sval[0] = '\0';
            inst->fields[fi].inst_id = -1; inst->fields[fi].arr_id = -1;
        }
        /* Apply initializer fields from the literal */
        for (int ci = 0; ci < n->child_count; ci++) {
            ASTNode *field_init = &rdx_nodes[n->children[ci]];
            JitVar val; my_memset(&val, 0, sizeof(JitVar));
            val.inst_id = -1; val.arr_id = -1;
            jit_eval_expr(jit, field_init->left, &val);
            for (int fi = 0; fi < inst->field_count; fi++) {
                if (my_strcmp(inst->fields[fi].name, field_init->sval) == 0) {
                    inst->fields[fi].ival = val.ival;
                    inst->fields[fi].dval = val.dval;
                    my_strcpy(inst->fields[fi].sval, val.sval);
                    my_strcpy(inst->fields[fi].type, val.type);
                    inst->fields[fi].inst_id = val.inst_id;
                    inst->fields[fi].arr_id  = val.arr_id;
                    break;
                }
            }
        }
        out_val->inst_id = jit->instance_count++;
        out_val->arr_id  = -1;
        my_strcpy(out_val->type, n->sval);
        my_strcpy(out_val->sval, n->sval);
    }
    /* ── Step 2: Member access  inst.field ── */
    else if (n->type == NODE_MEMBER_ACCESS) {
        JitVar *v = jit_get_var(jit, n->sval);
        if (!v || v->inst_id < 0) {
            jit_err(jit, n, "Not a struct: ", n->sval);
            jit->had_error = 1; return;
        }
        JitInstance *inst = &jit->instances[v->inst_id];
        for (int fi = 0; fi < inst->field_count; fi++) {
            if (my_strcmp(inst->fields[fi].name, n->rdx_type) == 0) {
                out_val->ival    = inst->fields[fi].ival;
                out_val->dval    = inst->fields[fi].dval;
                out_val->inst_id = inst->fields[fi].inst_id;
                out_val->arr_id  = inst->fields[fi].arr_id;
                my_strcpy(out_val->sval, inst->fields[fi].sval);
                my_strcpy(out_val->type, inst->fields[fi].type);
                return;
            }
        }
        jit_err(jit, n, "No such field on struct", NULL);
        jit->had_error = 1;
    }
    /* ── Step 3: Array literal  [1, 2, 3] ── */
    else if (n->type == NODE_ARRAY) {
        if (jit->array_count >= JIT_MAX_ARRAYS) {
            jit_err(jit, n, "Too many arrays", NULL); return;
        }
        JitArray *arr = &jit->arrays[jit->array_count];
        my_memset(arr, 0, sizeof(JitArray));
        arr->in_use = 1;
        arr->len = n->child_count;
        for (int ei = 0; ei < n->child_count && ei < JIT_MAX_ELEMS; ei++) {
            my_memset(&arr->elems[ei], 0, sizeof(JitVar));
            arr->elems[ei].inst_id = -1; arr->elems[ei].arr_id = -1;
            jit_eval_expr(jit, n->children[ei], &arr->elems[ei]);
        }
        out_val->arr_id  = jit->array_count++;
        out_val->inst_id = -1;
        my_strcpy(out_val->type, "array");
        int_to_str(out_val->arr_id, out_val->sval);
    }
    /* ── Step 3: Index access  arr[idx] ── */
    else if (n->type == NODE_INDEX) {
        JitVar *v = jit_get_var(jit, n->sval);
        if (!v || v->arr_id < 0) {
            jit_err(jit, n, "Not an array: ", n->sval);
            jit->had_error = 1; return;
        }
        JitVar idx_v; my_memset(&idx_v, 0, sizeof(JitVar)); idx_v.inst_id = -1; idx_v.arr_id = -1;
        jit_eval_expr(jit, n->left, &idx_v);
        JitArray *arr = &jit->arrays[v->arr_id];
        if (idx_v.ival < 0 || idx_v.ival >= arr->len) {
            jit_err(jit, n, "Array index out of bounds", NULL);
            jit->had_error = 1; return;
        }
        out_val->ival    = arr->elems[idx_v.ival].ival;
        out_val->dval    = arr->elems[idx_v.ival].dval;
        out_val->inst_id = arr->elems[idx_v.ival].inst_id;
        out_val->arr_id  = arr->elems[idx_v.ival].arr_id;
        my_strcpy(out_val->sval, arr->elems[idx_v.ival].sval);
        my_strcpy(out_val->type, arr->elems[idx_v.ival].type);
    }
    else {
        out_val->ival = 0; out_val->dval = 0.0;
        out_val->sval[0] = '\0'; my_strcpy(out_val->type, "unk");
        out_val->inst_id = -1; out_val->arr_id = -1;
    }
}

static void jit_emit_and_run(const char* final_string) {
    int str_len = my_strlen(final_string);
    int required_size = 68 + str_len;
    unsigned char* jit_mem = (unsigned char*)VirtualAlloc(NULL, required_size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!jit_mem) {
        jit_err(NULL, NULL, "VirtualAlloc failed.", NULL);
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
            JitVar temp; my_memset(&temp, 0, sizeof(JitVar)); temp.inst_id = -1; temp.arr_id = -1;
            jit_eval_expr(jit, n->left, &temp);
            v->ival    = temp.ival;
            v->dval    = temp.dval;
            v->inst_id = temp.inst_id;
            v->arr_id  = temp.arr_id;
            v->is_ok   = temp.is_ok;
            v->is_err  = temp.is_err;
            v->is_some = temp.is_some;
            v->is_none = temp.is_none;
            my_strcpy(v->sval, temp.sval);
            if (temp.type[0]) my_strcpy(v->type, temp.type);
        }
    }
    else if (n->type == NODE_ASSIGN) {
        /* Check if this is a struct field assignment: inst.field = val */
        if (n->rdx_type[0] != '\0') {
            /* struct field assignment: sval=inst_name, rdx_type=field_name */
            JitVar *v = jit_get_var(jit, n->sval);
            if (v && v->inst_id >= 0) {
                JitInstance *inst = &jit->instances[v->inst_id];
                JitVar val; my_memset(&val, 0, sizeof(JitVar)); val.inst_id = -1; val.arr_id = -1;
                jit_eval_expr(jit, n->left, &val);
                for (int fi = 0; fi < inst->field_count; fi++) {
                    if (my_strcmp(inst->fields[fi].name, n->rdx_type) == 0) {
                        inst->fields[fi].ival = val.ival;
                        inst->fields[fi].dval = val.dval;
                        my_strcpy(inst->fields[fi].sval, val.sval);
                        my_strcpy(inst->fields[fi].type, val.type);
                        break;
                    }
                }
                return;
            }
        }
        JitVar *v = jit_get_var(jit, n->sval);
        if (!v) { v = jit_add_var(jit, n->sval, "unk"); }
        if (v) {
            JitVar temp; my_memset(&temp, 0, sizeof(JitVar)); temp.inst_id = -1; temp.arr_id = -1;
            jit_eval_expr(jit, n->left, &temp);
            v->ival    = temp.ival;
            v->dval    = temp.dval;
            v->inst_id = temp.inst_id;
            v->arr_id  = temp.arr_id;
            v->is_ok   = temp.is_ok;
            v->is_err  = temp.is_err;
            v->is_some = temp.is_some;
            v->is_none = temp.is_none;
            my_strcpy(v->sval, temp.sval);
            my_strcpy(v->type, temp.type);
        }
    }
    /* ── Step 3: Array element assignment  arr[idx] = val ── */
    else if (n->type == NODE_ARRAY_ASSIGN) {
        JitVar *v = jit_get_var(jit, n->sval);
        if (!v || v->arr_id < 0) {
            jit_err(jit, n, "Not an array: ", n->sval);
            jit->had_error = 1; return;
        }
        JitVar idx_v; my_memset(&idx_v, 0, sizeof(JitVar)); idx_v.inst_id = -1; idx_v.arr_id = -1;
        jit_eval_expr(jit, n->left, &idx_v);
        JitArray *arr = &jit->arrays[v->arr_id];
        if (idx_v.ival < 0 || idx_v.ival >= arr->len) {
            jit_err(jit, n, "Array index out of bounds", NULL); return;
        }
        JitVar val; my_memset(&val, 0, sizeof(JitVar)); val.inst_id = -1; val.arr_id = -1;
        jit_eval_expr(jit, n->right, &val);
        arr->elems[idx_v.ival].ival = val.ival;
        arr->elems[idx_v.ival].dval = val.dval;
        arr->elems[idx_v.ival].inst_id = val.inst_id;
        arr->elems[idx_v.ival].arr_id  = val.arr_id;
        my_strcpy(arr->elems[idx_v.ival].sval, val.sval);
        my_strcpy(arr->elems[idx_v.ival].type, val.type);
    }
    else if (n->type == NODE_SYS_CALL) {
        usize max_len = 4096;
        char *out_buf = (char*)rdx_va_alloc(max_len);
        if (!out_buf) {
            jit_err(jit, n, "Failed to allocate sys call buffer", NULL);
            jit->had_error = 1;
            return;
        }
        out_buf[0] = '\0';
        int current_len = 0;
        
        /* 1st arg: base string */
        int base_len = my_strlen(n->sval);
        if (current_len + base_len >= (int)max_len - 10) {
            usize new_max = max_len * 2;
            while (current_len + base_len >= (int)new_max - 10) new_max *= 2;
            char *new_buf = (char*)rdx_va_alloc(new_max);
            if (!new_buf) {
                jit_err(jit, n, "Failed to reallocate sys call buffer", NULL);
                jit->had_error = 1;
                rdx_va_free(out_buf);
                return;
            }
            my_strcpy(new_buf, out_buf);
            rdx_va_free(out_buf);
            out_buf = new_buf;
            max_len = new_max;
        }
        my_strcat(out_buf, n->sval);
        current_len += base_len;
        
        /* 2nd arg: array of expressions */
        if (n->left >= 0) {
            ASTNode *arr = &rdx_nodes[n->left];
            for (int i = 0; i < arr->child_count; i++) {
                JitVar temp;
                jit_eval_expr(jit, arr->children[i], &temp);
                if (jit->had_error) { rdx_va_free(out_buf); return; }
                int temp_len = my_strlen(temp.sval);
                if (current_len + temp_len >= (int)max_len - 10) {
                    usize new_max = max_len * 2;
                    while (current_len + temp_len >= (int)new_max - 10) new_max *= 2;
                    char *new_buf = (char*)rdx_va_alloc(new_max);
                    if (!new_buf) {
                        jit_err(jit, n, "Failed to reallocate sys call buffer", NULL);
                        jit->had_error = 1;
                        rdx_va_free(out_buf);
                        return;
                    }
                    my_strcpy(new_buf, out_buf);
                    rdx_va_free(out_buf);
                    out_buf = new_buf;
                    max_len = new_max;
                }
                my_strcat(out_buf, temp.sval);
                current_len += temp_len;
            }
        }
        
        my_strcat(out_buf, "\n");
        
        /* Emit and Run */
        jit_emit_and_run(out_buf);
        rdx_va_free(out_buf);
    }
    else if (n->type == NODE_SYS_BLOCK || n->type == NODE_BLOCK) {
        int body = (n->type == NODE_SYS_BLOCK) ? n->body : idx;
        ASTNode *blk = &rdx_nodes[body];
        for (int i = 0; i < blk->child_count; i++) {
            if (jit->had_error || jit->return_flag || jit->break_flag || jit->continue_flag) break;
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
        if (my_strcmp(n->rdx_type, "range") == 0 && n->condition >= 0) {
            /* range loop:  loop start..end as i */
            ASTNode *range_arr = &rdx_nodes[n->condition];
            JitVar sv, ev;
            jit_eval_expr(jit, range_arr->children[0], &sv);
            jit_eval_expr(jit, range_arr->children[1], &ev);
            i64 start = sv.ival, end = ev.ival;
            /* loop variable name stored in sval */
            const char *ivar = n->sval[0] ? n->sval : "i";
            JitVar *loop_v = jit_add_var(jit, ivar, "num");
            for (i64 idx2 = start; idx2 < end && !jit->had_error && !jit->break_flag && !jit->return_flag; idx2++) {
                loop_v->ival = idx2;
                loop_v->dval = (double)idx2;
                int_to_str(idx2, loop_v->sval);
                jit->continue_flag = 0;
                jit_exec_stmt(jit, n->body);
                if (jit->continue_flag) { jit->continue_flag = 0; continue; }
            }
            jit->break_flag = 0;
        } else if (n->condition < 0) {
            /* infinite loop — break via break_flag */
            while (!jit->had_error && !jit->break_flag && !jit->return_flag) {
                jit->continue_flag = 0;
                jit_exec_stmt(jit, n->body);
                if (jit->continue_flag) { jit->continue_flag = 0; continue; }
            }
            jit->break_flag = 0;
        } else {
            /* while-style: loop cond { } */
            while (!jit->break_flag && !jit->return_flag) {
                JitVar cond;
                jit_eval_expr(jit, n->condition, &cond);
                if (jit->had_error || cond.ival == 0) break;
                jit->continue_flag = 0;
                jit_exec_stmt(jit, n->body);
                if (jit->continue_flag) { jit->continue_flag = 0; continue; }
            }
            jit->break_flag = 0;
        }
    }
    else if (n->type == NODE_RETURN) {
        /* ret expr — also handles break sentinel (ival == -999) */
        if (n->ival == -999) {
            /* break statement */
            jit->break_flag = 1;
            return;
        }
        /* Evaluate return expression and store it; set unwind flag */
        my_memset(&jit->ret_val, 0, sizeof(JitVar));
        if (n->left >= 0) {
            jit_eval_expr(jit, n->left, &jit->ret_val);
        }
        jit->return_flag = 1;
    }
    else if (n->type == NODE_FN_CALL) {
        /* Function call as a statement — nl(), or any side-effect call.
           Special built-ins handled here directly for statement context. */
        if (my_strcmp(n->sval, "nl") == 0 || my_strcmp(n->sval, "newline") == 0) {
            jit_emit_and_run("\n");
        } else if (my_strcmp(n->sval, "nl") == 0) {
            jit_emit_and_run("\n");
        } else {
            /* Evaluate for side effects (e.g. user-defined fn placeholders) */
            JitVar discard;
            jit_eval_expr(jit, idx, &discard);
        }
    }
    else if (n->type == NODE_GET) {
        /* get() as standalone statement — discard the result */
        JitVar discard;
        jit_eval_expr(jit, idx, &discard);
    }
    else if (n->type == NODE_CONTINUE) {
        /* Signal the loop executor to skip to the next iteration */
        jit->continue_flag = 1;
    }
    else if (n->type == NODE_SAFE_FAIL) {
        /* safe { body } fail errname { handler }
           Save error state, run safe body. If error occurred, reset and run fail body. */
        int prev_error = jit->had_error;
        jit->had_error = 0;

        /* run safe body */
        jit_exec_stmt(jit, n->body);

        if (jit->had_error) {
            /* error in safe block — set the error variable name if provided */
            jit->had_error = 0; /* reset so fail body can run cleanly */
            if (n->sval[0] != '\0') {
                /* store a string variable with the error description */
                JitVar *errv = jit_add_var(jit, n->sval, "chr");
                if (errv) my_strcpy(errv->sval, "runtime error");
            }
            if (n->else_body >= 0) {
                jit_exec_stmt(jit, n->else_body);
            }
        } else {
            /* restore outer error state */
            jit->had_error = prev_error;
        }
    }
    else if (n->type == NODE_MATCH) {
        /* match val { pat -> { body } ... _ -> { body } }
           Evaluate the subject, then iterate arms pairwise (pat, body). */
        JitVar subject;
        jit_eval_expr(jit, n->condition, &subject);

        int matched = 0;
        int i = 0;
        while (i + 1 < n->child_count && !matched) {
            int pat_idx  = n->children[i];
            int body_idx = n->children[i + 1];
            i += 2;

            ASTNode *pat = &rdx_nodes[pat_idx];

            /* wildcard _ always matches */
            if (pat->type == NODE_LITERAL_STR && my_strcmp(pat->sval, "_") == 0) {
                jit_exec_stmt(jit, body_idx);
                matched = 1;
                break;
            }

            /* evaluate pattern and compare */
            JitVar pv;
            jit_eval_expr(jit, pat_idx, &pv);

            int arm_match = 0;
            if (my_strcmp(subject.type, "chr") == 0 || my_strcmp(pv.type, "chr") == 0) {
                arm_match = (my_strcmp(subject.sval, pv.sval) == 0);
            } else {
                arm_match = (subject.ival == pv.ival);
            }

            if (arm_match) {
                jit_exec_stmt(jit, body_idx);
                matched = 1;
            }
        }
    }
}

/* ─── Module loading helper (Step 6) ─────────────────────────────── */
/* Forward-declare so jit_run_module can call jit_run recursively */
static int jit_run_module(JitEngine *jit, const char *path, int depth);

static int jit_run_module(JitEngine *jit, const char *path, int depth) {
    if (depth > 8) {
        jit_err(jit, NULL, "Module import depth limit reached", NULL);
        return -1;
    }
    /* Build path: try as-is first, then append .rdx */
    char full_path[512];
    my_strncpy(full_path, path, 507);
    /* Check if already ends in .rdx */
    usize plen = my_strlen(full_path);
    if (plen < 4 ||
        full_path[plen-4] != '.' || full_path[plen-3] != 'r' ||
        full_path[plen-2] != 'd' || full_path[plen-1] != 'x') {
        my_strcat(full_path, ".rdx");
    }

    usize src_len = 0;
    char *src = rdx_read_file(full_path, &src_len);
    if (!src) {
        jit_err(jit, NULL, "Cannot load module: ", full_path);
        return -1;
    }

    /* Lex — lexer_init already allocates token buffer via rdx_alloc_bump */
    Lexer modL;
    lexer_init(&modL, src);
    int mod_tok_count = lexer_tokenize(&modL);
    if (modL.had_error || mod_tok_count < 0) {
        jit_err(jit, NULL, "Lex error in module: ", full_path);
        return -1;
    }

    /* Parse the module into the global node pool (appending) */
    Parser modP;
    parser_init(&modP, modL.tokens, modL.token_count);

    int mod_root = parser_parse_append(&modP);
    if (modP.had_error) {
        jit_err(jit, NULL, "Parse error in module: ", full_path);
        return -1;
    }

    /* Run the module (which may recursively load more) */
    ASTNode *prog = &rdx_nodes[mod_root];
    /* Pass 1: register fns, shapes, and nested loads */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_FN_DECL) {
            if (jit->fn_count < JIT_MAX_FNS) {
                JitFn *fn = &jit->fns[jit->fn_count++];
                my_strncpy(fn->name, n->sval, 63);
                fn->node_idx = prog->children[i];
            }
        } else if (n->type == NODE_EXTERN) {
            if (jit->extern_fn_count < JIT_MAX_EXTERN_FNS) {
                JitExternFn *efn = &jit->extern_fns[jit->extern_fn_count++];
                my_strncpy(efn->name, n->op, 63);
                my_strncpy(efn->lib_name, n->sval, 127);
                efn->node_idx = prog->children[i];
                efn->fn_ptr = NULL;
            }
        } else if (n->type == NODE_BLUEPRINT) {
            if (jit->shape_count < JIT_MAX_SHAPES) {
                JitShape *sh = &jit->shapes[jit->shape_count++];
                my_strncpy(sh->shape_name, n->sval, 63);
                sh->field_count = n->child_count;
                for (int fi = 0; fi < n->child_count && fi < JIT_MAX_FIELDS; fi++) {
                    ASTNode *fd = &rdx_nodes[n->children[fi]];
                    my_strncpy(sh->field_names[fi], fd->sval, 63);
                    my_strncpy(sh->field_types[fi], fd->rdx_type, 63);
                }
            }
        } else if (n->type == NODE_LOAD) {
            jit_run_module(jit, n->sval, depth + 1);
        }
    }
    /* Pass 2: execute top-level statements (not decls) */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_FN_DECL || n->type == NODE_EXTERN || n->type == NODE_BLUEPRINT || n->type == NODE_LOAD) continue;
        jit_exec_stmt(jit, prog->children[i]);
        if (jit->had_error) break;
    }

    return jit->had_error ? -1 : 0;
}

/* ============================================================
 *  §4  Public API
 * ============================================================ */
void jit_init(JitEngine *jit) {
    my_memset(jit, 0, sizeof(JitEngine));
    /* seed boolean literals (both cases) */
    JitVar *t = jit_add_var(jit, "true",  "blo"); t->ival = 1; my_strcpy(t->sval, "1");
    JitVar *T = jit_add_var(jit, "TRUE",  "blo"); T->ival = 1; my_strcpy(T->sval, "1");
    JitVar *f = jit_add_var(jit, "false", "blo"); f->ival = 0; my_strcpy(f->sval, "0");
    JitVar *F = jit_add_var(jit, "FALSE", "blo"); F->ival = 0; my_strcpy(F->sval, "0");
}

int jit_run(JitEngine *jit, int root) {
    ASTNode *prog = &rdx_nodes[root];

    /* Pass 1: Register all top-level function, shape, and load declarations */
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_FN_DECL) {
            if (jit->fn_count < JIT_MAX_FNS) {
                JitFn *fn = &jit->fns[jit->fn_count++];
                my_strncpy(fn->name, n->sval, 63);
                fn->node_idx = prog->children[i];
            }
        } else if (n->type == NODE_EXTERN) {
            if (jit->extern_fn_count < JIT_MAX_EXTERN_FNS) {
                JitExternFn *efn = &jit->extern_fns[jit->extern_fn_count++];
                my_strncpy(efn->name, n->op, 63);
                my_strncpy(efn->lib_name, n->sval, 127);
                efn->node_idx = prog->children[i];
                efn->fn_ptr = NULL;
            }
        } else if (n->type == NODE_BLUEPRINT) {
            /* Register shape definition */
            if (jit->shape_count < JIT_MAX_SHAPES) {
                JitShape *sh = &jit->shapes[jit->shape_count++];
                my_strncpy(sh->shape_name, n->sval, 63);
                sh->field_count = n->child_count;
                for (int fi = 0; fi < n->child_count && fi < JIT_MAX_FIELDS; fi++) {
                    ASTNode *fd = &rdx_nodes[n->children[fi]];
                    my_strncpy(sh->field_names[fi], fd->sval, 63);
                    my_strncpy(sh->field_types[fi], fd->rdx_type, 63);
                }
            }
        } else if (n->type == NODE_IMPL_BLOCK) {
            /* Register each method (NODE_FN_DECL) from the impl block */
            for (int mi = 0; mi < n->child_count; mi++) {
                if (jit->fn_count < JIT_MAX_FNS) {
                    JitFn *jf = &jit->fns[jit->fn_count++];
                    ASTNode *mnode = &rdx_nodes[n->children[mi]];
                    my_strncpy(jf->name, mnode->sval, 63); /* "ShapeName::method" */
                    jf->node_idx = n->children[mi];
                }
            }
        } else if (n->type == NODE_LOAD) {
            /* Load and integrate external module */
            jit_run_module(jit, n->sval, 1);
        }
    }

    /* Pass 2: Execute all non-declaration statements */
    int executed_stmts = 0;
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *n = &rdx_nodes[prog->children[i]];
        if (n->type == NODE_FN_DECL || n->type == NODE_EXTERN || n->type == NODE_BLUEPRINT
            || n->type == NODE_IMPL_BLOCK || n->type == NODE_LOAD) continue;
        jit_exec_stmt(jit, prog->children[i]);
        executed_stmts++;
        if (jit->had_error) break;
    }

    /* Pass 3: If the program was entirely declarations (C-style), auto-call main() */
    if (!jit->had_error && executed_stmts == 0) {
        for (int i = 0; i < jit->fn_count; i++) {
            if (my_strcmp(jit->fns[i].name, "main") == 0) {
                /* Synthesise a zero-argument call to main */
                ASTNode dummy;
                my_memset(&dummy, 0, sizeof(dummy));
                dummy.type = NODE_FN_CALL;
                my_strcpy(dummy.sval, "main");
                dummy.child_count = 0;
                /* Use a scratch node index — reuse slot 0 of rdx_nodes which is
                   the NODE_PROGRAM root that we no longer need to walk */
                int scratch = 0;
                rdx_nodes[scratch] = dummy;
                jit_exec_stmt(jit, scratch);
                break;
            }
        }
    }

    return jit->had_error ? -1 : 0;
}

