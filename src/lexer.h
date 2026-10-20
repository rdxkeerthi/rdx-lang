/*
 * ============================================================================
 *  lexer.h  —  RDX Hand-Rolled Lexical Analyzer
 * ============================================================================
 */

#ifndef RDX_LEXER_H
#define RDX_LEXER_H

#include "core.h"
#include "token.h"

typedef struct {
    const char *source;
    const char *cursor;
    const char *start;
    usize       source_len;
    u32         line;
    u32         col;
    const char *line_start;
    const char *filename;
} Lexer;

void  lexer_init(Lexer *lexer, const char *source, usize length, const char *filename);
Token lexer_next(Lexer *lexer);
Token lexer_peek(Lexer *lexer);

#endif /* RDX_LEXER_H */
