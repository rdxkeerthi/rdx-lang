/*
 * ============================================================================
 *  rdx_diag.h  —  RDX "Where & Why" Human-First Diagnostic Engine
 * ============================================================================
 */

#ifndef RDX_DIAG_NEW_H
#define RDX_DIAG_NEW_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char *filename;
    const char *source;
    size_t      source_len;
    int         error_count;
} RdxDiagContext;

void rdx_diag_set_context(const char *filename, const char *source, size_t len);

void rdx_diag_report(const char *filename,
                     int line,
                     int col,
                     int token_len,
                     const char *title,
                     const char *why_explanation,
                     const char *fix_suggestion);

const char *rdx_diag_suggest_similar(const char *word, const char **candidates, int num_candidates);

#endif /* RDX_DIAG_NEW_H */
