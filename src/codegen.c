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

/* forward declaration — defined in §3 */
static void jit_emit_and_run(const char *final_string);

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

/* Write a string directly (no JIT trampoline) — used for prompts */
static void direct_write(const char *s) {
    if (!s || !s[0]) return;
    void *hOut = GetStdHandle((unsigned int)-11);  /* STD_OUTPUT_HANDLE */
    unsigned int written = 0;
    WriteFile(hOut, s, (unsigned int)my_strlen(s), &written, NULL);
}

/* Read one line of user input from stdin into buf (up to max_chars).
   Also prints the prompt string first.
   Returns number of chars read (excluding newline). */
static int jit_read_line(const char *prompt, char *buf, int max_chars) {
    /* Print prompt */
    if (prompt && prompt[0]) direct_write(prompt);

    void *hIn = GetStdHandle((unsigned int)-10);  /* STD_INPUT_HANDLE */

    unsigned int chars_read = 0;
    /* ReadConsoleA needs console mode — enable line input */
    SetConsoleMode(hIn, 0x0003);  /* ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT */
    ReadConsoleA(hIn, buf, (unsigned int)(max_chars - 1), &chars_read, NULL);

    /* Strip trailing \r\n */
    int len = (int)chars_read;
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) len--;
    buf[len] = '\0';
    return len;
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
            jit_eval_expr(jit, n->children[i], &args[argc++]);
        }
        out_val->ival = 0; out_val->dval = 0.0; out_val->sval[0] = '\0';
        my_strcpy(out_val->type, "num");

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
            int_to_str(out_val->ival, out_val->sval);
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

        /* ─── print / println aliases ─── */
        } else if (my_strcmp(fn, "print") == 0 && argc >= 1) {
            jit_emit_and_run(args[0].sval);
            out_val->ival = 0; out_val->sval[0] = '\0';
        } else if (my_strcmp(fn, "println") == 0 && argc >= 1) {
            char tmp[520];
            my_strcpy(tmp, args[0].sval);
            my_strcat(tmp, "\n");
            jit_emit_and_run(tmp);
            out_val->ival = 0; out_val->sval[0] = '\0';

        /* ─── string utilities ─── */
        } else if (my_strcmp(fn, "chr_at") == 0 && argc >= 2) {
            /* chr_at(str, idx) -> single char string */
            int si = (int)args[1].ival;
            int slen = (int)my_strlen(args[0].sval);
            if (si >= 0 && si < slen) {
                out_val->sval[0] = args[0].sval[si];
                out_val->sval[1] = '\0';
            } else {
                out_val->sval[0] = '\0';
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

        } else {
            /* Unknown function — silently return 0 so user-defined fns
               (future dispatch table) don't crash the JIT */
            out_val->ival = 0; out_val->sval[0] = '\0';
        }
        return;
    }
    else {
        out_val->ival = 0; out_val->dval = 0.0;
        out_val->sval[0] = '\0'; my_strcpy(out_val->type, "unk");
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
            for (i64 idx2 = start; idx2 < end && !jit->had_error && !jit->break_flag; idx2++) {
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
            while (!jit->had_error && !jit->break_flag) {
                jit->continue_flag = 0;
                jit_exec_stmt(jit, n->body);
                if (jit->continue_flag) { jit->continue_flag = 0; continue; }
            }
            jit->break_flag = 0;
        } else {
            /* while-style: loop cond { } */
            while (!jit->break_flag) {
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
        if (n->left >= 0) {
            JitVar temp;
            jit_eval_expr(jit, n->left, &temp);
        }
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
    
    for (int i = 0; i < prog->child_count; i++) {
        jit_exec_stmt(jit, prog->children[i]);
    }
    
    return jit->had_error ? -1 : 0;
}
