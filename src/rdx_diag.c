/*
 * ============================================================================
 *  rdx_diag.c  —  RDX "Where & Why" Human-First Diagnostic Engine Implementation
 * ============================================================================
 */

#include "rdx_diag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static RdxDiagContext g_diag = {0};

void rdx_diag_set_context(const char *filename, const char *source, size_t len) {
    g_diag.filename = filename ? filename : "<input>";
    g_diag.source = source;
    g_diag.source_len = len;
    g_diag.error_count = 0;
}

static int levenshtein_dist(const char *s1, const char *s2) {
    int len1 = (int)strlen(s1);
    int len2 = (int)strlen(s2);
    if (len1 > 64) len1 = 64;
    if (len2 > 64) len2 = 64;

    int d[65][65];
    for (int i = 0; i <= len1; i++) d[i][0] = i;
    for (int j = 0; j <= len2; j++) d[0][j] = j;

    for (int j = 1; j <= len2; j++) {
        for (int i = 1; i <= len1; i++) {
            int cost = (tolower((unsigned char)s1[i - 1]) == tolower((unsigned char)s2[j - 1])) ? 0 : 1;
            int del = d[i - 1][j] + 1;
            int ins = d[i][j - 1] + 1;
            int sub = d[i - 1][j - 1] + cost;
            int min = (del < ins) ? del : ins;
            d[i][j] = (min < sub) ? min : sub;
        }
    }
    return d[len1][len2];
}

const char *rdx_diag_suggest_similar(const char *word, const char **candidates, int num_candidates) {
    if (!word || !candidates || num_candidates <= 0) return NULL;
    int best_dist = 999;
    const char *best_match = NULL;

    for (int i = 0; i < num_candidates; i++) {
        if (!candidates[i]) continue;
        int d = levenshtein_dist(word, candidates[i]);
        if (d < best_dist && d <= 3) {
            best_dist = d;
            best_match = candidates[i];
        }
    }
    return best_match;
}

void rdx_diag_report(const char *filename,
                     int line,
                     int col,
                     int token_len,
                     const char *title,
                     const char *why_explanation,
                     const char *fix_suggestion) {
    g_diag.error_count++;

    const char *fn = filename ? filename : (g_diag.filename ? g_diag.filename : "<script>");
    if (token_len <= 0) token_len = 1;
    if (col <= 0) col = 1;
    if (line <= 0) line = 1;

    fprintf(stderr, "\n");
    fprintf(stderr, "\033[1;31m╭─── [RDX DIAGNOSTIC: %s] ────────────────────────────────────────\033[0m\n",
            title ? title : "ERROR");
    fprintf(stderr, "│ File:     %s\n", fn);
    fprintf(stderr, "│ Location: Line %d, Column %d\n", line, col);
    fprintf(stderr, "├────────────────────────────────────────────────────────────────────────\n");

    /* Extract the exact line from source */
    const char *src = g_diag.source;
    if (src) {
        int cur_line = 1;
        const char *p = src;
        const char *line_start = p;

        while (*p && cur_line < line) {
            if (*p == '\n') {
                cur_line++;
                line_start = p + 1;
            }
            p++;
        }

        if (cur_line == line) {
            const char *line_end = line_start;
            while (*line_end && *line_end != '\n' && *line_end != '\r') {
                line_end++;
            }
            int line_len = (int)(line_end - line_start);

            /* Print line number and code */
            fprintf(stderr, "│ %4d |  ", line);
            for (int i = 0; i < line_len; i++) {
                fputc(line_start[i], stderr);
            }
            fprintf(stderr, "\n");

            /* Print the caret pointer ^^^ */
            fprintf(stderr, "│      |  ");
            for (int i = 1; i < col && i <= line_len; i++) {
                if (line_start[i - 1] == '\t') fputc('\t', stderr);
                else fputc(' ', stderr);
            }
            fprintf(stderr, "\033[1;33m");
            for (int i = 0; i < token_len; i++) {
                fputc('^', stderr);
            }
            fprintf(stderr, "\033[0m\n");
        }
    }

    fprintf(stderr, "├────────────────────────────────────────────────────────────────────────\n");
    fprintf(stderr, "│ \033[1;36mWHERE:\033[0m Line %d, Column %d in '%s'\n", line, col, fn);
    fprintf(stderr, "│ \033[1;31mWHY:\033[0m   %s\n", why_explanation ? why_explanation : "Unspecified syntax or runtime error");
    if (fix_suggestion && fix_suggestion[0]) {
        fprintf(stderr, "│ \033[1;32mFIX:\033[0m   %s\n", fix_suggestion);
    }
    fprintf(stderr, "\033[1;31m╰────────────────────────────────────────────────────────────────────────\033[0m\n\n");
}
