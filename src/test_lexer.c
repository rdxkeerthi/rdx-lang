/*
 * test_lexer.c - Lexer test harness
 */
#define RDX_DEFINE_ENTRY
#include "lexer.h"

extern char* __stdcall GetCommandLineA(void);

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
        rdx_print_err("Usage: test_lexer <file>\n");
        return 1;
    }

    usize len = 0;
    char *source = rdx_read_file(argv[1], &len);
    if (!source) {
        rdx_print_err("Failed to read file\n");
        return 1;
    }

    Lexer lex;
    lexer_init(&lex, source);
    int tokens = lexer_tokenize(&lex);
    if (tokens < 0) {
        rdx_print_err("Lexing failed\n");
    } else {
        lexer_dump(&lex);
    }

    return 0;
}
