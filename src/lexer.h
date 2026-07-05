/*
 * lexer.h — RDX Tokeniser: Public API  (v2 — simplified syntax)
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
    TOK_FLT_LIT,       /* 1.2           */

    /* Type keywords  (3-letter data types) */
    TOK_NUM,           /* num           */
    TOK_CHR,           /* chr           */
    TOK_FLT,           /* flt           */
    TOK_BLO,           /* blo           */
    TOK_DOB,           /* dob           */
    TOK_BYT,           /* byt           */

    /* Statement keywords */
    TOK_MAKE,          /* make  — function declaration   */
    TOK_RET,           /* ret   — return value           */
    TOK_IF,            /* if                             */
    TOK_ELIF,          /* elif                           */
    TOK_ELSE,          /* else                           */
    TOK_LOOP,          /* loop                           */
    TOK_BREAK,         /* break                          */
    TOK_WHEN,          /* when  — pattern match          */
    TOK_SHAPE,         /* shape — struct / blueprint     */
    TOK_FIX,           /* fix   — compile-time constant  */
    TOK_MUT,           /* mut   — mutable variable       */
    TOK_LOAD,          /* load  — import module          */
    TOK_PUB,           /* pub   — public export          */
    TOK_AS,            /* as    — loop range alias       */
    TOK_IN,            /* in    — range / iterator       */
    TOK_AND,           /* and   — logical and            */
    TOK_OR,            /* or    — logical or             */
    TOK_NOT,           /* not   — logical not            */
    TOK_TRUE,          /* true  — boolean literal        */
    TOK_FALSE,         /* false — boolean literal        */
    TOK_ALLOC,         /* alloc — raw heap alloc         */
    TOK_DROP,          /* drop  — explicit free          */
    TOK_SYS,           /* sys   — entry point marker     */
    TOK_CALL,          /* call  — print / output         */
    TOK_GET,           /* get   — read input from user   */
    TOK_SAFE,          /* safe  — safe block (try)       */
    TOK_FAIL,          /* fail  — fail handler (catch)   */
    TOK_MATCH,         /* match — pattern match          */
    TOK_CASE,          /* case  — match arm              */
    TOK_CONTINUE,      /* continue — skip loop iteration */

    /* Structured Error Handling (Step 5) */
    TOK_OK,            /* ok    — success result         */
    TOK_ERR,           /* err   — error result           */
    TOK_SOME,          /* some  — optional with value    */
    TOK_NONE,          /* none  — empty optional         */
    TOK_QUESTION,      /* ?     — propagate operator     */
    TOK_EXTERN,        /* extern — C FFI                 */

    /* Identifier */
    TOK_IDENT,

    /* Arithmetic */
    TOK_PLUS,          /* +   */
    TOK_MINUS,         /* -   */
    TOK_STAR,          /* *   */
    TOK_SLASH,         /* /   */
    TOK_PERCENT,       /* %   */

    /* Comparison */
    TOK_EQ,            /* ==  */
    TOK_NEQ,           /* !=  */
    TOK_LT,            /* <   */
    TOK_GT,            /* >   */
    TOK_LTE,           /* <=  */
    TOK_GTE,           /* >=  */

    /* Assignment */
    TOK_ASSIGN,        /* =   */
    TOK_ARROW,         /* ->  */
    TOK_DOTDOT,        /* ..  (range: 0..10) */

    /* Punctuation */
    TOK_LPAREN,        /* (   */
    TOK_RPAREN,        /* )   */
    TOK_LBRACE,        /* {   */
    TOK_RBRACE,        /* }   */
    TOK_LBRACKET,      /* [   */
    TOK_RBRACKET,      /* ]   */
    TOK_SEMICOLON,     /* ;   */
    TOK_COMMA,         /* ,   */
    TOK_DOT,           /* .   */
    TOK_COLON,         /* :   */

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
    int       col;          /* 1-based source column      */
} Token;

/* ============================================================
 *  Lexer State
 * ============================================================ */
typedef struct {
    const char *source;
    usize       pos;
    usize       length;
    int         line;
    usize       line_start_pos;
    Token      *tokens;
    int         token_count;
    int         had_error;
} Lexer;

/* ============================================================
 *  Public API
 * ============================================================ */
void        lexer_init     (Lexer *L, const char *source);
int         lexer_tokenize (Lexer *L);
const char *token_type_name(TokenType t);
void        lexer_dump     (const Lexer *L);

/* Helper: is this token one of the 3-letter type keywords? */
static inline bool tok_is_type(TokenType t) {
    return t == TOK_NUM || t == TOK_CHR || t == TOK_FLT ||
           t == TOK_BLO || t == TOK_DOB || t == TOK_BYT;
}

#endif /* RDX_LEXER_H */
