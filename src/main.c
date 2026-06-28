/*
 * main.c — RDX Compiler Entry Point
 *
 * Usage: rdxc.exe <source.rdx> [-o output.asm]
 *
 * Compile:
 *   gcc -nostdlib -O2 -o rdxc.exe src/main.c src/lexer.c src/parser.c src/codegen.c -lkernel32
 */

#define RDX_DEFINE_ENTRY
#include "codegen.h"   /* pulls in parser.h -> lexer.h -> core.h */

/* ---- Command-line helper (GetCommandLineA-free argv parse) ----
   MinGW -nostdlib doesn't give us argc/argv; we get them via
   the PE TEB / kernel call. For simplicity we use a pre-split
   approach: scan the raw string ourselves.                       */
extern char* __stdcall GetCommandLineA(void);

/* Split raw Win32 command line into argv[].
   Returns argc. Modifies cmdline in-place. */
static int split_cmdline(char *cmdline, char **argv, int max_args) {
    int argc = 0;
    char *p  = cmdline;
    while (*p && argc < max_args) {
        /* skip spaces */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        /* quoted token */
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

/* ---- rdx_compiler_main — called by mainCRTStartup in core.h -- */
int rdx_compiler_main(void) {
    /* Parse command line */
    char raw[2048];
    my_strncpy(raw, GetCommandLineA(), 2047);
    raw[2047] = '\0';

    char *argv[16];
    int   argc = split_cmdline(raw, argv, 16);

    if (argc < 2) {
        rdx_print_err("RDX Compiler v0.1 (Win64)\r\n");
        rdx_print_err("Usage: rdx <source.rdx>       (Compiles to .exe)\r\n");
        rdx_print_err("       rdx run <source.rdx>   (Compiles and runs immediately)\r\n");
        return 1;
    }

    int is_run = 0;
    const char *src_path = argv[1];
    
    if (my_strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            rdx_print_err("[rdx] Missing source file for run\r\n");
            return 1;
        }
        is_run = 1;
        src_path = argv[2];
    }

    // Determine base name from src_path
    char base_name[256];
    my_strncpy(base_name, src_path, 255);
    base_name[255] = '\0';
    
    // Find last '.'
    char *dot = NULL;
    for (int i = 0; base_name[i]; i++) {
        if (base_name[i] == '.') dot = &base_name[i];
    }
    // Only strip extension if it's .rdx (or simply the last extension)
    if (dot) *dot = '\0';

    char asm_path[256];
    char obj_path[256];
    char exe_path[256];
    
    if (is_run) {
        my_strcpy(asm_path, "rdx_tmp.asm");
        my_strcpy(obj_path, "rdx_tmp.obj");
        my_strcpy(exe_path, "rdx_tmp.exe");
    } else {
        my_strcpy(asm_path, base_name); my_strcat(asm_path, ".asm");
        my_strcpy(obj_path, base_name); my_strcat(obj_path, ".obj");
        my_strcpy(exe_path, base_name); my_strcat(exe_path, ".exe");
    }

    /* ---- Phase 1: Read source file ---- */
    rdx_print("[rdx] Reading "); rdx_print(src_path); rdx_print("\r\n");
    usize src_len = 0;
    char *source  = rdx_read_file(src_path, &src_len);
    if (!source) { rdx_print_err("[rdx] Failed to read source\r\n"); return 1; }

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

    /* ---- Phase 4: Code generation ---- */
    rdx_print("[rdx] Generating Win64 NASM...\r\n");
    /* CodeGen is ~265 KB — must be heap-allocated, not stack */
    CodeGen *cg = (CodeGen*)rdx_alloc_bump(sizeof(CodeGen));
    if (!cg) { rdx_print_err("[rdx] OOM for CodeGen\r\n"); return 1; }
    codegen_init(cg);
    int cg_ret = codegen_run(cg, root);
    if (cg_ret < 0 || cg->had_error) {
        rdx_print_err("[rdx] Codegen errors — aborting\r\n"); return 1;
    }
    rdx_print("[rdx] "); rdx_print_num((i64)cg->out.pos);
    rdx_print(" bytes of NASM emitted\r\n");

    /* ---- Phase 5: Write output and Compile ---- */
    rdx_print("[rdx] Writing "); rdx_print(asm_path); rdx_print("\r\n");
    if (rdx_write_file(asm_path, cg->out.buf, cg->out.pos) != 0) {
        rdx_print_err("[rdx] Failed to write output\r\n"); return 1;
    }

    char cmd[512];
    
    rdx_print("[rdx] Assembling...\r\n");
    my_strcpy(cmd, "nasm -f win64 "); my_strcat(cmd, asm_path); my_strcat(cmd, " -o "); my_strcat(cmd, obj_path);
    if (rdx_run_cmd(cmd) != 0) {
        rdx_print_err("[rdx] NASM failed\r\n");
        return 1;
    }
    
    rdx_print("[rdx] Linking...\r\n");
    my_strcpy(cmd, "ld "); my_strcat(cmd, obj_path); my_strcat(cmd, " -lkernel32 -o "); my_strcat(cmd, exe_path);
    if (rdx_run_cmd(cmd) != 0) {
        rdx_print_err("[rdx] Linker failed\r\n");
        return 1;
    }
    
    if (is_run) {
        rdx_print("[rdx] Running...\r\n");
        int exit_code = rdx_run_cmd(exe_path);
        /* Cleanup */
        DeleteFileA(asm_path);
        DeleteFileA(obj_path);
        DeleteFileA(exe_path);
        return exit_code;
    } else {
        /* Cleanup intermediate files, leave executable */
        DeleteFileA(asm_path);
        DeleteFileA(obj_path);
        rdx_print("[rdx] Successfully compiled to "); rdx_print(exe_path); rdx_print("\r\n");
    }

    return 0;
}
