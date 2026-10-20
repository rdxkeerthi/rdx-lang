/*
 * ============================================================================
 *  parser.h  —  RDX Pratt Parser (Top-Down Operator Precedence)
 * ============================================================================
 */

#ifndef RDX_PARSER_H
#define RDX_PARSER_H

#include "core.h"
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "arena.h"

typedef enum {
    PREC_NONE = 0,
    PREC_ASSIGNMENT,  /* = += -= *= /= */
    PREC_OR,          /* or ||          */
    PREC_AND,         /* and &&         */
    PREC_BITWISE_OR,  /* |              */
    PREC_BITWISE_XOR, /* ^              */
    PREC_BITWISE_AND, /* &              */
    PREC_EQUALITY,    /* == !=          */
    PREC_COMPARISON,  /* < <= > >=      */
    PREC_SHIFT,       /* << >>          */
    PREC_TERM,        /* + -            */
    PREC_FACTOR,      /* * / %          */
    PREC_UNARY,       /* ! - ~ not      */
    PREC_CALL,        /* () [] .        */
    PREC_PRIMARY
} Precedence;

typedef struct {
    Lexer       lexer;
    Token       current;
    Token       previous;
    Arena      *arena;
    bool        had_error;
    bool        panic_mode;
    const char *error_msg;
} Parser;

void        parser_init(Parser *parser, const char *source, usize length, const char *filename, Arena *arena);
AstProgram *parser_parse_program(Parser *parser);
AstExpr    *parser_parse_expr(Parser *parser, Precedence precedence);
AstStmt    *parser_parse_stmt(Parser *parser);
AstDecl    *parser_parse_decl(Parser *parser);

#endif /* RDX_PARSER_H */
