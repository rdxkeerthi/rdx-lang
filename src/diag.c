/*
 * diag.c — RDX Diagnostics Engine
 */
#include "diag.h"

static const char *g_source = NULL;

void rdx_diag_init(const char *source) {
    g_source = source;
}

void rdx_diag_error(int line, int col, int len, const char *prefix, const char *msg) {
    /* [PREFIX] line X, col Y: msg */
    rdx_print_err(prefix);
    rdx_print_err(" line "); rdx_print_num_err((i64)line);
    rdx_print_err(", col "); rdx_print_num_err((i64)col);
    rdx_print_err(": "); rdx_print_err(msg);
    rdx_print_err("\r\n");

    if (g_source == NULL) return;

    /* Scan source for the correct line */
    const char *p = g_source;
    int cur_line = 1;
    while (*p && cur_line < line) {
        if (*p == '\n') cur_line++;
        p++;
    }

    if (cur_line == line) {
        /* Print the line text */
        const char *line_start = p;
        rdx_print_err("    ");
        while (*p && *p != '\n' && *p != '\r') {
            rdx_print_char(*p);
            p++;
        }
        rdx_print_err("\r\n");

        /* Print the caret */
        rdx_print_err("    ");
        for (int i = 1; i < col; i++) {
            /* If the line had a tab, we probably want to print a tab to match alignment, but spaces are fine for basic */
            if (line_start[i-1] == '\t') rdx_print_char('\t');
            else rdx_print_char(' ');
        }
        rdx_print_err("^");
        for (int i = 1; i < len; i++) {
            rdx_print_err("~");
        }
        rdx_print_err("\r\n");
    }
}

void rdx_diag_errorf(int line, int col, int len, const char *prefix, const char *msg, const char *arg) {
    char full_msg[256];
    full_msg[0] = '\0';
    
    if (arg) {
        const char *fmt = msg;
        char *out = full_msg;
        int space = 255;
        int replaced = 0;
        
        while (*fmt && space > 0) {
            if (fmt[0] == '%' && fmt[1] == 's') {
                const char *a = arg;
                while (*a && space > 0) {
                    *out++ = *a++;
                    space--;
                }
                fmt += 2; /* skip %s */
                replaced = 1;
            } else {
                *out++ = *fmt++;
                space--;
            }
        }
        if (!replaced) {
            const char *a = arg;
            while (*a && space > 0) {
                *out++ = *a++;
                space--;
            }
        }
        *out = '\0';
    } else {
        my_strncpy(full_msg, msg, 255);
        full_msg[255] = '\0';
    }
    
    rdx_diag_error(line, col, len, prefix, full_msg);
}
