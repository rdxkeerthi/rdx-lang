/*
 * lexer.h — RDX Tokeniser: Public API
 * Target : x86-64 Windows, MinGW -nostdlib -lkernel32
 * Dep    : core.h only
 */
#ifndef RDX_LEXER_H
#define RDX_LEXER_H

#include "core.h"

/* ============================================================
 *  Token Types
 * ============================================================ */
typedef enum {
    /* Literals */
    TOK_NUM_LIT = 0,   /* 42            */
    TOK_STR_LIT,       /* "hello"       */

    /* Type keywords */
    TOK_NUM,           /* num           */
    TOK_STR,           /* str           */

    /* Statement keywords */
    TOK_FN,            /* fn            */
    TOK_RETURN,        /* return        */
    TOK_IF,            /* if            */
    TOK_ELSE,          /* else          */
    TOK_LOOP,          /* loop          */
    TOK_BLUEPRINT,     /* blueprint     */
    TOK_ALLOC,         /* alloc         */
    TOK_DROP,          /* drop          */
    TOK_SYS,           /* sys           */
    TOK_CALL,          /* call          */

    /* Identifier */
    TOK_IDENT,

    /* Arithmetic operators */
    TOK_PLUS,          /* +             */
    TOK_MINUS,         /* -             */
    TOK_STAR,          /* *             */
    TOK_SLASH,         /* /             */

    /* Comparison operators */
    TOK_EQ,            /* ==            */
    TOK_NEQ,           /* !=            */
    TOK_LT,            /* <             */
    TOK_GT,            /* >             */
    TOK_LTE,           /* <=            */
    TOK_GTE,           /* >=            */

    /* Assignment / declaration */
    TOK_ASSIGN,        /* :             */
    TOK_ARROW,         /* ->            */

    /* Punctuation */
    TOK_LPAREN,        /* (             */
    TOK_RPAREN,        /* )             */
    TOK_LBRACE,        /* {             */
    TOK_RBRACE,        /* }             */
    TOK_SEMICOLON,     /* ;             */
    TOK_COMMA,         /* ,             */
    TOK_DOT,           /* .             */

    /* Sentinels */
    TOK_EOF,
    TOK_UNKNOWN,

    TOK_COUNT
} TokenType;

/* ============================================================
 *  Token
 * ============================================================ */
typedef struct {
    TokenType type;
    char      value[256];   /* raw lexeme, NUL-terminated */
    int       line;         /* 1-based source line        */
} Token;

/* ============================================================
 *  Lexer State
 * ============================================================ */
typedef struct {
    const char *source;         /* NUL-terminated source text  */
    usize       pos;            /* current char index          */
    usize       length;         /* source byte count           */
    int         line;           /* current 1-based line        */
    Token      *tokens;         /* bump-allocated token array  */
    int         token_count;
    int         had_error;
} Lexer;

/* ============================================================
 *  Public API
 * ============================================================ */

/* Initialise lexer over `source`. Allocates token array from bump heap. */
void        lexer_init     (Lexer *L, const char *source);

/* Tokenise entire source. Returns token count or -1 on fatal error. */
int         lexer_tokenize (Lexer *L);

/* Return a static display name for a token type. */
const char *token_type_name(TokenType t);

/* Debug: print all tokens to stdout. */
void        lexer_dump     (const Lexer *L);

#endif /* RDX_LEXER_H */
