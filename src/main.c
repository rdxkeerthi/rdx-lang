/*
 * main.c — RDX Compiler Entry Point
 *
 * Usage: rdx <source.rdx>
 *
 * Compile:
 *   gcc -nostdlib -O2 -o rdx.exe src/main.c src/lexer.c src/parser.c src/codegen.c -lkernel32
 */

#define RDX_DEFINE_ENTRY
#include "codegen.h"   /* pulls in parser.h -> lexer.h -> core.h */
#include "aot.h"
#include "diag.h"
#include "analyzer.h"

/* ---- Command-line helper (GetCommandLineA-free argv parse) ---- */
extern char* __stdcall GetCommandLineA(void);

/* GCC implicitly emits calls to memcpy and memset for struct assignment/initialization */
void *memcpy(void *dst, const void *src, usize n) {
    return my_memcpy(dst, src, n);
}
void *memset(void *dst, i32 c, usize n) {
    return my_memset(dst, c, n);
}

static int split_cmdline(char *cmdline, char **argv, int max_args) {
    int argc = 0;
    char *p  = cmdline;
    while (*p && argc < max_args) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p == '"') {
            p++;
            argv[argc++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
    }
    return argc;
}

int rdx_compiler_main(void) {
    char raw[2048];
    my_strncpy(raw, GetCommandLineA(), 2047);
    raw[2047] = '\0';

    char *argv[16];
    int   argc = split_cmdline(raw, argv, 16);

    if (argc < 2) {
        rdx_print_err("RDX Compiler JIT v0.2 (Win64)\r\n");
        rdx_print_err("Usage: rdx <source.rdx>\r\n");
        return 1;
    }

    const char *src_path = argv[1];
    int is_transpile = 0;
    
    if (my_strcmp(argv[1], "run") == 0 && argc >= 3) {
        src_path = argv[2];
    } else if (my_strcmp(argv[1], "transpile") == 0 && argc >= 3) {
        src_path = argv[2];
        is_transpile = 1;
    }

    /* ---- Phase 1: Read source file ---- */
    rdx_print("[rdx] Reading "); rdx_print(src_path); rdx_print("\r\n");
    usize src_len = 0;
    char *source  = rdx_read_file(src_path, &src_len);
    if (!source) { rdx_print_err("[rdx] Failed to read source\r\n"); return 1; }

    rdx_diag_init(source);

    /* ---- Phase 2: Lex ---- */
    rdx_print("[rdx] Lexing...\r\n");
    Lexer L;
    lexer_init(&L, source);
    int tok_count = lexer_tokenize(&L);
    if (tok_count < 0 || L.had_error) {
        rdx_print_err("[rdx] Lex errors — aborting\r\n"); return 1;
    }
    rdx_print("[rdx] "); rdx_print_num(tok_count);
    rdx_print(" tokens\r\n");

    /* ---- Phase 3: Parse ---- */
    rdx_print("[rdx] Parsing...\r\n");
    Parser P;
    parser_init(&P, L.tokens, tok_count);
    int root = parser_parse(&P);
    if (root < 0 || P.had_error) {
        rdx_print_err("[rdx] Parse errors — aborting\r\n"); return 1;
    }
    rdx_print("[rdx] "); rdx_print_num(rdx_node_count);
    rdx_print(" AST nodes\r\n");

    /* ---- Phase 3b: Semantic Ownership Analysis ---- */
    rdx_print("[rdx] Semantic Analysis...\r\n");
    int sa_res = rdx_analyze_ownership(root);
    rdx_print("[rdx] Semantic Analysis completed with code "); rdx_print_num(sa_res); rdx_print("\r\n");
    if (sa_res != 0) {
        rdx_print_err("[rdx] Memory safety errors detected — aborting\r\n");
        return 1;
    }

    /* ---- Phase 4: Execute / Transpile ---- */
    if (is_transpile) {
        rdx_print("[rdx] Transpiling...\r\n");
        int ret = aot_transpile(root, "out.c");
        if (ret < 0) {
            rdx_print_err("[rdx] Transpile failed.\r\n"); return 1;
        }
    } else {
        rdx_print("[rdx] JIT Executing...\r\n");
        rdx_print("[rdx] sizeof(JitEngine) = "); rdx_print_num((i64)sizeof(JitEngine)); rdx_print("\r\n");
        extern u8 *rdx_bump_ptr, *rdx_bump_base;
        rdx_print("[rdx] Bump used = "); rdx_print_num((i64)(rdx_bump_ptr - rdx_bump_base)); rdx_print("\r\n");
        JitEngine *jit = (JitEngine*)rdx_alloc_bump(sizeof(JitEngine));
        if (!jit) { rdx_print_err("[rdx] OOM for JitEngine\r\n"); return 1; }
        
        jit_init(jit);
        int ret = jit_run(jit, root);
        if (ret < 0 || jit->had_error) {
            rdx_print_err("[rdx] JIT errors during execution.\r\n"); return 1;
        }
        
        rdx_print("[rdx] Execution completed successfully.\r\n");
    }

    return 0;
}

