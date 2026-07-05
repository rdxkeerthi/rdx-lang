/* aot.c — RDX AST-to-C transpiler
 *
 * Translates a parsed RDX AST into a standalone C source file that can be
 * compiled with any C99 compiler and run as a native binary.
 *
 * Design rule: never silently emit a "/* UNIMPLEMENTED *\/" comment for an
 * unrecognised node — report a real error and abort so the user never gets
 * a false-success binary that produces no output.
 */
#include "aot.h"
#include "core.h"
#include "diag.h"

/* ---- String buffer ---- */
typedef struct {
    char  *buf;
    usize  cap;
    usize  len;
    int    had_error; /* set when capacity is exceeded or a node is unsupported */
} StringBuf;

static void sb_init(StringBuf *sb, usize cap) {
    sb->buf       = (char *)rdx_va_alloc(cap);
    sb->cap       = cap;
    sb->len       = 0;
    sb->had_error = 0;
    if (sb->buf) sb->buf[0] = '\0';
}

static void aot_err(ASTNode *n, const char *msg, const char *arg) {
    if (n) rdx_diag_errorf(n->line, n->col, 1, "[AOT ERROR]", msg, arg);
    else rdx_diag_errorf(0, 0, 0, "[AOT ERROR]", msg, arg);
}

static void sb_append(StringBuf *sb, const char *str) {
    if (!sb->buf || !str || sb->had_error) return;
    usize slen = my_strlen(str);
    if (sb->len + slen + 1 > sb->cap) {
        aot_err(NULL, "StringBuf capacity exceeded!", NULL);
        sb->had_error = 1;
        return;
    }
    my_memcpy(sb->buf + sb->len, str, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sb_append_num(StringBuf *sb, i64 num) {
    char nbuf[32];
    if (num == 0) { sb_append(sb, "0"); return; }
    i64 temp = num < 0 ? -num : num;
    int idx = 30;
    nbuf[31] = '\0';
    while (temp > 0 && idx >= 0) {
        nbuf[idx--] = '0' + (char)(temp % 10);
        temp /= 10;
    }
    if (num < 0 && idx >= 0) nbuf[idx--] = '-';
    sb_append(sb, &nbuf[idx + 1]);
}

/* ---- Forward declaration ---- */
static void aot_emit_node(StringBuf *sb, int node_idx, int stmt_ctx);

/* ---- Type helper ---- */
static void aot_emit_type(StringBuf *sb, const char *rdx_type) {
    if (my_strcmp(rdx_type, "num")  == 0 ||
        my_strcmp(rdx_type, "bool") == 0) {
        sb_append(sb, "int64_t ");
    } else if (my_strcmp(rdx_type, "flt") == 0) {
        sb_append(sb, "double ");
    } else if (my_strcmp(rdx_type, "chr") == 0 ||
               my_strcmp(rdx_type, "str") == 0) {
        sb_append(sb, "const char *");
    } else if (rdx_type[0]) {
        /* unknown type — emit as int64_t with a comment */
        sb_append(sb, "int64_t /* ");
        sb_append(sb, rdx_type);
        sb_append(sb, " */ ");
    } else {
        sb_append(sb, "int64_t ");
    }
}

/* ---- NODE_SYS_CALL emitter
 *
 * RDX:   call("prefix " [expr1, expr2])
 *        call(expr)
 *
 * Becomes:  rdx_print_str("prefix ");
 *           rdx_print_auto(<expr1>);
 *           rdx_print_auto(<expr2>);
 *           rdx_print_str("\n");
 *
 * We use our own rdx_print_str / rdx_print_i64 / rdx_print_f64 helpers
 * emitted in the prelude so we do not depend on <stdio.h> internals.
 */
static void aot_emit_sys_call(StringBuf *sb, int node_idx) {
    ASTNode *node = &rdx_nodes[node_idx];

    /* Base string prefix (may be empty) */
    if (node->sval[0]) {
        sb_append(sb, "    rdx_print_str(\"");
        /* emit the string with basic C escaping */
        const char *p = node->sval;
        while (*p) {
            if (*p == '"')       { sb_append(sb, "\\\""); }
            else if (*p == '\\') { sb_append(sb, "\\\\"); }
            else if (*p == '\n') { sb_append(sb, "\\n");  }
            else if (*p == '\r') { sb_append(sb, "\\r");  }
            else {
                char ch[2] = { *p, '\0' };
                sb_append(sb, ch);
            }
            p++;
        }
        sb_append(sb, "\");\n");
    }

    /* Optional argument array */
    if (node->left >= 0) {
        ASTNode *arr = &rdx_nodes[node->left];
        if (arr->type == NODE_ARRAY) {
            for (int i = 0; i < arr->child_count; i++) {
                sb_append(sb, "    rdx_print_auto(");
                aot_emit_node(sb, arr->children[i], 0);
                sb_append(sb, ");\n");
            }
        } else {
            /* single expression argument (call(expr)) */
            sb_append(sb, "    rdx_print_auto(");
            aot_emit_node(sb, node->left, 0);
            sb_append(sb, ");\n");
        }
    }

    /* Trailing newline */
    sb_append(sb, "    rdx_print_str(\"\\n\");\n");
}

/* ---- Main node emitter
 *
 * stmt_ctx = 1 when the node appears at statement level (needs semicolon).
 * stmt_ctx = 0 when used as an expression (no trailing semicolon).
 */
static void aot_emit_node(StringBuf *sb, int node_idx, int stmt_ctx) {
    if (node_idx < 0 || sb->had_error) return;
    ASTNode *node = &rdx_nodes[node_idx];

    switch (node->type) {

        /* ── Program / Block ─────────────────────────────────────── */
        case NODE_PROGRAM:
            for (int i = 0; i < node->child_count; i++) {
                aot_emit_node(sb, node->children[i], 1);
                sb_append(sb, "\n");
            }
            break;

        case NODE_BLOCK:
            for (int i = 0; i < node->child_count; i++) {
                aot_emit_node(sb, node->children[i], 1);
            }
            break;

        /* sys.rdx { body } — just emit the body statements */
        case NODE_SYS_BLOCK:
            if (node->body >= 0)
                aot_emit_node(sb, node->body, 0);
            break;

        /* ── Declarations ────────────────────────────────────────── */
        case NODE_VAR_DECL:
            sb_append(sb, "    ");
            aot_emit_type(sb, node->rdx_type[0] ? node->rdx_type : "num");
            sb_append(sb, node->sval);
            sb_append(sb, " = ");
            aot_emit_node(sb, node->left, 0);
            sb_append(sb, ";\n");
            break;

        case NODE_ASSIGN:
            sb_append(sb, "    ");
            sb_append(sb, node->sval);
            if (node->rdx_type[0]) {
                /* struct-field assign: var.field = val */
                sb_append(sb, ".");
                sb_append(sb, node->rdx_type);
            }
            sb_append(sb, " = ");
            aot_emit_node(sb, node->left, 0);
            sb_append(sb, ";\n");
            break;

        case NODE_FN_DECL: {
            /* return type */
            if (node->rdx_type[0]) {
                aot_emit_type(sb, node->rdx_type);
            } else {
                sb_append(sb, "void ");
            }
            /* function name (main -> main_rdx to avoid clash) */
            if (my_strcmp(node->sval, "main") == 0)
                sb_append(sb, "main_rdx");
            else
                sb_append(sb, node->sval);
            sb_append(sb, "(");
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) sb_append(sb, ", ");
                ASTNode *param = &rdx_nodes[node->children[i]];
                aot_emit_type(sb, param->rdx_type[0] ? param->rdx_type : "num");
                sb_append(sb, param->sval);
            }
            sb_append(sb, ") {\n");
            aot_emit_node(sb, node->body, 0);
            sb_append(sb, "}\n");
            break;
        }

        /* ── Control flow ────────────────────────────────────────── */
        case NODE_RETURN:
            if (node->ival == -999) {
                /* break sentinel */
                sb_append(sb, "    break;\n");
            } else {
                sb_append(sb, "    return ");
                if (node->left >= 0) aot_emit_node(sb, node->left, 0);
                sb_append(sb, ";\n");
            }
            break;

        case NODE_CONTINUE:
            sb_append(sb, "    continue;\n");
            break;

        case NODE_IF:
            sb_append(sb, "    if (");
            aot_emit_node(sb, node->condition, 0);
            sb_append(sb, ") {\n");
            aot_emit_node(sb, node->body, 0);
            sb_append(sb, "    }");
            if (node->else_body >= 0) {
                sb_append(sb, " else {\n");
                aot_emit_node(sb, node->else_body, 0);
                sb_append(sb, "    }");
            }
            sb_append(sb, "\n");
            break;

        case NODE_LOOP:
            if (my_strcmp(node->rdx_type, "range") == 0 && node->condition >= 0) {
                /* range loop: loop start..end as i */
                ASTNode *range = &rdx_nodes[node->condition];
                const char *ivar = node->sval[0] ? node->sval : "i";
                sb_append(sb, "    for (int64_t ");
                sb_append(sb, ivar);
                sb_append(sb, " = ");
                aot_emit_node(sb, range->children[0], 0);
                sb_append(sb, "; ");
                sb_append(sb, ivar);
                sb_append(sb, " < ");
                aot_emit_node(sb, range->children[1], 0);
                sb_append(sb, "; ");
                sb_append(sb, ivar);
                sb_append(sb, "++) {\n");
                aot_emit_node(sb, node->body, 0);
                sb_append(sb, "    }\n");
            } else if (node->condition < 0) {
                /* infinite loop */
                sb_append(sb, "    while (1) {\n");
                aot_emit_node(sb, node->body, 0);
                sb_append(sb, "    }\n");
            } else {
                /* while-style */
                sb_append(sb, "    while (");
                aot_emit_node(sb, node->condition, 0);
                sb_append(sb, ") {\n");
                aot_emit_node(sb, node->body, 0);
                sb_append(sb, "    }\n");
            }
            break;

        /* ── Calls & output ──────────────────────────────────────── */
        case NODE_SYS_CALL:
            /* Statement-level call(); the emitter appends its own semicolons */
            aot_emit_sys_call(sb, node_idx);
            break;

        case NODE_FN_CALL: {
            const char *fname = node->sval;
            if (stmt_ctx) sb_append(sb, "    ");
            if (my_strcmp(fname, "main") == 0)
                sb_append(sb, "main_rdx");
            else
                sb_append(sb, fname);
                sb_append(sb, "(");
                for (int i = 0; i < node->child_count; i++) {
                    if (i > 0) sb_append(sb, ", ");
                    aot_emit_node(sb, node->children[i], 0);
                }
                sb_append(sb, ")");

            if (stmt_ctx) sb_append(sb, ";\n");
            break;
        }

        /* ── Expressions ─────────────────────────────────────────── */
        case NODE_BINARY_OP: {
            const char *op = node->op;
            /* map RDX operators to C equivalents */
            const char *c_op = op;
            if (my_strcmp(op, "and") == 0) c_op = "&&";
            else if (my_strcmp(op, "or")  == 0) c_op = "||";
            else if (my_strcmp(op, "not") == 0) {
                sb_append(sb, "(!");
                aot_emit_node(sb, node->left, 0);
                sb_append(sb, ")");
                break;
            } else if (my_strcmp(op, "neg") == 0) {
                sb_append(sb, "(-");
                aot_emit_node(sb, node->left, 0);
                sb_append(sb, ")");
                break;
            }
            sb_append(sb, "(");
            aot_emit_node(sb, node->left, 0);
            sb_append(sb, " ");
            sb_append(sb, c_op);
            sb_append(sb, " ");
            aot_emit_node(sb, node->right, 0);
            sb_append(sb, ")");
            break;
        }

        case NODE_LITERAL_NUM:
            sb_append_num(sb, node->ival);
            break;

        case NODE_LITERAL_STR:
            sb_append(sb, "\"");
            sb_append(sb, node->sval);
            sb_append(sb, "\"");
            break;

        case NODE_LITERAL_FLT:
            sb_append(sb, node->sval);
            break;

        case NODE_IDENT:
            sb_append(sb, node->sval);
            break;

        case NODE_MEMBER_ACCESS:
            sb_append(sb, node->sval);
            sb_append(sb, ".");
            sb_append(sb, node->rdx_type);
            break;

        case NODE_ARRAY: {
            /* Emit as a C compound literal — works for simple initialisation */
            sb_append(sb, "{");
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) sb_append(sb, ", ");
                aot_emit_node(sb, node->children[i], 0);
            }
            sb_append(sb, "}");
            break;
        }

        /* ── Unsupported — loud error, never silent ──────────────── */
        default: {
            /*
             * Naming the node type in the error message is the key fix for the
             * "false-success" bug: the transpiler now aborts rather than
             * emitting a comment that compiles and runs but produces no output.
             */
            static const char *names[] = {
                "PROGRAM","VAR_DECL","FN_DECL","RETURN","IF","LOOP","BLUEPRINT",
                "ALLOC","DROP","ASSIGN","FN_CALL","SYS_CALL","BINARY_OP",
                "LIT_NUM","LIT_STR","LIT_FLT","IDENT","SYS_BLOCK","MEMBER_ACCESS",
                "BLOCK","ARRAY","GET","SAFE_FAIL","MATCH","CONTINUE","STRUCT_LIT",
                "INDEX","ARRAY_ASSIGN","LOAD","RESULT","PROPAGATE","EXTERN"
            };
            const char *name = (node->type >= 0 && node->type <= NODE_EXTERN)
                               ? names[node->type] : "UNKNOWN";
            aot_err(node, "Unsupported AST node type '", name);
            aot_err(node, "' — cannot transpile this construct yet.", NULL);
            aot_err(node, "         Falling back to JIT execution is recommended.", NULL);
            sb->had_error = 1;
            break;
        }
    }
}

/* ---- C prelude emitted before the transpiled program ---- */
static const char *aot_prelude =
    "#include <stdint.h>\n"
    "#include <stdio.h>\n"
    "\n"
    "/* RDX runtime helpers */\n"
    "static void rdx_print_str(const char *s) { fputs(s, stdout); }\n"
    "static void rdx_print_i64(int64_t n)     { printf(\"%lld\", (long long)n); }\n"
    "static void rdx_print_f64(double d)      { printf(\"%g\", d); }\n"
    "\n"
    "/* rdx_print_auto: default to integer printing.\n"
    "   The AOT transpiler currently emits int64_t for all numeric types.\n"
    "   String and float variants are called explicitly in generated code. */\n"
    "#define rdx_print_auto(x) rdx_print_i64((int64_t)(x))\n"
    "\n";

/* ---- Public entry point ---- */
int aot_transpile(int root_node, const char *out_path) {
    StringBuf sb;
    sb_init(&sb, 2 * 1024 * 1024); /* 2 MB */
    if (!sb.buf) {
        aot_err(NULL, "Failed to allocate output buffer", NULL);
        return -1;
    }

    sb_append(&sb, aot_prelude);
    aot_emit_node(&sb, root_node, 1);

    if (sb.had_error) {
        aot_err(NULL, "Transpilation failed — see errors above.", NULL);
        aot_err(NULL, "Hint: run with 'rdx run file.rdx' to use the JIT interpreter instead.", NULL);
        return -1;
    }

    /* Entry-point wrapper that calls the user's main() */
    sb_append(&sb, "\nint main(void) {\n    main_rdx();\n    return 0;\n}\n");

    i32 bytes_written = rdx_write_file(out_path, sb.buf, sb.len);
    if (bytes_written < 0) {
        aot_err(NULL, "Failed to write output file: ", out_path);
        return -1;
    }

    rdx_print("[AOT] Transpiled successfully to ");
    rdx_print(out_path);
    rdx_print("\r\n");
    return 0;
}
