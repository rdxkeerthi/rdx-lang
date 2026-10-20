/*
 * ============================================================================
 *  rdx_engine.c  —  RDX Syntax-Free Execution Engine & Cyber Forensics Runtime
 * ============================================================================
 */

#include "rdx_engine.h"
#include "rdx_cyber.h"
#include "rdx_diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ============================================================================
 *  1. Lexical Analysis (Tokens)
 * ============================================================================ */

typedef enum {
    TOK_EOF = 0,
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_IDENT,
    TOK_LET,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_FN,
    TOK_RETURN,
    TOK_PRINT,
    TOK_ALERT,
    TOK_TIMELINE,
    TOK_TRUE,
    TOK_FALSE,
    TOK_NIL,

    /* Operators & Delimiters */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_AND,
    TOK_OR,
    TOK_LT,
    TOK_LTE,
    TOK_GT,
    TOK_GTE,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_NEWLINE
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;
    int         length;
    int         line;
    int         col;
    int64_t     ival;
    double      fval;
    char        sval[512];
} RdxToken;

typedef struct {
    const char *source;
    size_t      source_len;
    const char *cursor;
    int         line;
    int         col;
    const char *filename;
} RdxLexer;

static void lexer_init(RdxLexer *lex, const char *filename, const char *source) {
    lex->source = source;
    lex->source_len = strlen(source);
    lex->cursor = source;
    lex->line = 1;
    lex->col = 1;
    lex->filename = filename;
}

static char lexer_peek(RdxLexer *lex) {
    return *lex->cursor;
}

static char lexer_advance(RdxLexer *lex) {
    char c = *lex->cursor++;
    if (c == '\n') {
        lex->line++;
        lex->col = 1;
    } else {
        lex->col++;
    }
    return c;
}

static bool lexer_match(RdxLexer *lex, char expected) {
    if (*lex->cursor == expected) {
        lexer_advance(lex);
        return true;
    }
    return false;
}

static void lexer_skip_whitespace_and_comments(RdxLexer *lex) {
    while (*lex->cursor) {
        char c = *lex->cursor;
        if (c == ' ' || c == '\t' || c == '\r') {
            lexer_advance(lex);
        } else if (c == '/' && lex->cursor[1] == '/') {
            /* Single-line comment */
            while (*lex->cursor && *lex->cursor != '\n') {
                lexer_advance(lex);
            }
        } else if (c == '#') {
            /* Bash/Python style single-line comment */
            while (*lex->cursor && *lex->cursor != '\n') {
                lexer_advance(lex);
            }
        } else if (c == '/' && lex->cursor[1] == '*') {
            /* Multi-line comment */
            lexer_advance(lex);
            lexer_advance(lex);
            while (*lex->cursor && !(*lex->cursor == '*' && lex->cursor[1] == '/')) {
                lexer_advance(lex);
            }
            if (*lex->cursor == '*') {
                lexer_advance(lex);
                lexer_advance(lex);
            }
        } else {
            break;
        }
    }
}

static RdxToken lexer_next_token(RdxLexer *lex) {
    lexer_skip_whitespace_and_comments(lex);

    RdxToken tok;
    memset(&tok, 0, sizeof(tok));
    tok.start = lex->cursor;
    tok.line = lex->line;
    tok.col = lex->col;

    if (!*lex->cursor) {
        tok.type = TOK_EOF;
        tok.length = 0;
        return tok;
    }

    char c = lexer_advance(lex);

    if (c == '\n') {
        tok.type = TOK_NEWLINE;
        tok.length = 1;
        return tok;
    }

    /* Single / Double character operators */
    switch (c) {
        case '+': tok.type = TOK_PLUS; tok.length = 1; return tok;
        case '-': tok.type = TOK_MINUS; tok.length = 1; return tok;
        case '*': tok.type = TOK_STAR; tok.length = 1; return tok;
        case '/': tok.type = TOK_SLASH; tok.length = 1; return tok;
        case '%': tok.type = TOK_PERCENT; tok.length = 1; return tok;
        case ';': tok.type = TOK_SEMICOLON; tok.length = 1; return tok;
        case ',': tok.type = TOK_COMMA; tok.length = 1; return tok;
        case '(': tok.type = TOK_LPAREN; tok.length = 1; return tok;
        case ')': tok.type = TOK_RPAREN; tok.length = 1; return tok;
        case '{': tok.type = TOK_LBRACE; tok.length = 1; return tok;
        case '}': tok.type = TOK_RBRACE; tok.length = 1; return tok;
        case '[': tok.type = TOK_LBRACKET; tok.length = 1; return tok;
        case ']': tok.type = TOK_RBRACKET; tok.length = 1; return tok;
        case '=':
            if (lexer_match(lex, '=')) { tok.type = TOK_EQ; tok.length = 2; }
            else { tok.type = TOK_ASSIGN; tok.length = 1; }
            return tok;
        case '!':
            if (lexer_match(lex, '=')) { tok.type = TOK_NEQ; tok.length = 2; return tok; }
            break;
        case '&':
            if (lexer_match(lex, '&')) { tok.type = TOK_AND; tok.length = 2; return tok; }
            tok.type = TOK_AND; tok.length = 1; return tok;
        case '|':
            if (lexer_match(lex, '|')) { tok.type = TOK_OR; tok.length = 2; return tok; }
            tok.type = TOK_OR; tok.length = 1; return tok;
        case '<':
            if (lexer_match(lex, '=')) { tok.type = TOK_LTE; tok.length = 2; }
            else { tok.type = TOK_LT; tok.length = 1; }
            return tok;
        case '>':
            if (lexer_match(lex, '=')) { tok.type = TOK_GTE; tok.length = 2; }
            else { tok.type = TOK_GT; tok.length = 1; }
            return tok;
    }

    /* String Literal */
    if (c == '"' || c == '\'') {
        char quote = c;
        char buf[512];
        int bidx = 0;
        int start_col = tok.col;
        int start_line = tok.line;

        while (*lex->cursor && *lex->cursor != quote) {
            char ch = lexer_advance(lex);
            if (ch == '\\') {
                char esc = lexer_advance(lex);
                if (esc == 'n') ch = '\n';
                else if (esc == 't') ch = '\t';
                else if (esc == 'r') ch = '\r';
                else if (esc == '\\') ch = '\\';
                else if (esc == quote) ch = quote;
            }
            if (bidx < 511) buf[bidx++] = ch;
        }

        if (!*lex->cursor) {
            rdx_diag_report(lex->filename, start_line, start_col, 1,
                            "UNTERMINATED STRING",
                            "String literal reached end-of-file without a closing quote.",
                            "Add a closing quote '\"' to terminate your string.");
            tok.type = TOK_EOF;
            return tok;
        }

        lexer_advance(lex); /* consume closing quote */
        buf[bidx] = '\0';
        tok.type = TOK_STR;
        tok.length = (int)(lex->cursor - tok.start);
        strncpy(tok.sval, buf, sizeof(tok.sval) - 1);
        return tok;
    }

    /* Number Literal (Hex or Decimal or Float) */
    if (isdigit((unsigned char)c)) {
        const char *num_start = tok.start;
        bool is_float = false;

        if (c == '0' && (*lex->cursor == 'x' || *lex->cursor == 'X')) {
            lexer_advance(lex); /* consume 'x' */
            while (isxdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
            tok.type = TOK_INT;
            tok.ival = strtoll(num_start, NULL, 16);
            tok.length = (int)(lex->cursor - num_start);
            return tok;
        }

        while (isdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
        if (*lex->cursor == '.' && isdigit((unsigned char)lex->cursor[1])) {
            is_float = true;
            lexer_advance(lex); /* consume '.' */
            while (isdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
        }

        tok.length = (int)(lex->cursor - num_start);
        if (is_float) {
            tok.type = TOK_FLOAT;
            tok.fval = strtod(num_start, NULL);
        } else {
            tok.type = TOK_INT;
            tok.ival = strtoll(num_start, NULL, 10);
        }
        return tok;
    }

    /* Identifiers, Dot-Names (e.g. ip.in_subnet), Keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        const char *id_start = tok.start;
        while (isalnum((unsigned char)*lex->cursor) || *lex->cursor == '_' || *lex->cursor == '.') {
            lexer_advance(lex);
        }
        tok.length = (int)(lex->cursor - id_start);
        if (tok.length >= 512) tok.length = 511;
        memcpy(tok.sval, id_start, tok.length);
        tok.sval[tok.length] = '\0';

        /* Keyword resolution */
        if (strcmp(tok.sval, "let") == 0) tok.type = TOK_LET;
        else if (strcmp(tok.sval, "if") == 0) tok.type = TOK_IF;
        else if (strcmp(tok.sval, "else") == 0) tok.type = TOK_ELSE;
        else if (strcmp(tok.sval, "while") == 0) tok.type = TOK_WHILE;
        else if (strcmp(tok.sval, "for") == 0) tok.type = TOK_FOR;
        else if (strcmp(tok.sval, "in") == 0) tok.type = TOK_IN;
        else if (strcmp(tok.sval, "fn") == 0) tok.type = TOK_FN;
        else if (strcmp(tok.sval, "return") == 0) tok.type = TOK_RETURN;
        else if (strcmp(tok.sval, "print") == 0 || strcmp(tok.sval, "println") == 0) tok.type = TOK_PRINT;
        else if (strcmp(tok.sval, "alert") == 0) tok.type = TOK_ALERT;
        else if (strcmp(tok.sval, "timeline") == 0) tok.type = TOK_TIMELINE;
        else if (strcmp(tok.sval, "and") == 0) tok.type = TOK_AND;
        else if (strcmp(tok.sval, "or") == 0) tok.type = TOK_OR;
        else if (strcmp(tok.sval, "true") == 0) { tok.type = TOK_TRUE; tok.ival = 1; }
        else if (strcmp(tok.sval, "false") == 0) { tok.type = TOK_FALSE; tok.ival = 0; }
        else if (strcmp(tok.sval, "nil") == 0 || strcmp(tok.sval, "none") == 0) tok.type = TOK_NIL;
        else tok.type = TOK_IDENT;

        return tok;
    }

    /* Unrecognized character */
    char why[128];
    snprintf(why, sizeof(why), "Unexpected or illegal character '%c' (ASCII %d)", c, (int)c);
    rdx_diag_report(lex->filename, tok.line, tok.col, 1,
                    "SYNTAX ERROR", why,
                    "Remove the illegal character or verify your syntax.");
    tok.type = TOK_EOF;
    return tok;
}

/* ============================================================================
 *  2. AST & Values
 * ============================================================================ */

static RdxVal val_nil(void) {
    RdxVal v; v.type = VAL_NIL; return v;
}
static RdxVal val_bool(bool b) {
    RdxVal v; v.type = VAL_BOOL; v.as.bval = b; return v;
}
static RdxVal val_int(int64_t i) {
    RdxVal v; v.type = VAL_INT; v.as.ival = i; return v;
}
static RdxVal val_float(double f) {
    RdxVal v; v.type = VAL_FLOAT; v.as.fval = f; return v;
}
static RdxVal val_str(const char *s) {
    RdxVal v; v.type = VAL_STR;
    v.as.sval = s ? strdup(s) : strdup("");
    return v;
}
static RdxVal val_list(void) {
    RdxVal v; v.type = VAL_LIST;
    v.as.lval = (RdxList*)malloc(sizeof(RdxList));
    v.as.lval->items = NULL;
    v.as.lval->count = 0;
    v.as.lval->capacity = 0;
    return v;
}

static void list_append(RdxList *list, RdxVal val) {
    if (list->count >= list->capacity) {
        int new_cap = (list->capacity < 4) ? 4 : list->capacity * 2;
        list->items = (RdxVal*)realloc(list->items, new_cap * sizeof(RdxVal));
        list->capacity = new_cap;
    }
    list->items[list->count++] = val;
}

static void val_print(RdxVal v) {
    switch (v.type) {
        case VAL_NIL: printf("nil"); break;
        case VAL_BOOL: printf(v.as.bval ? "true" : "false"); break;
        case VAL_INT: printf("%lld", (long long)v.as.ival); break;
        case VAL_FLOAT: printf("%.6g", v.as.fval); break;
        case VAL_STR: printf("%s", v.as.sval); break;
        case VAL_LIST: {
            printf("[");
            for (int i = 0; i < v.as.lval->count; i++) {
                if (i > 0) printf(", ");
                if (v.as.lval->items[i].type == VAL_STR) {
                    printf("\"%s\"", v.as.lval->items[i].as.sval);
                } else {
                    val_print(v.as.lval->items[i]);
                }
            }
            printf("]");
            break;
        }
        case VAL_FN: printf("<fn %s>", v.as.fnval->name); break;
    }
}

static bool val_is_truthy(RdxVal v) {
    if (v.type == VAL_NIL) return false;
    if (v.type == VAL_BOOL) return v.as.bval;
    if (v.type == VAL_INT) return v.as.ival != 0;
    if (v.type == VAL_FLOAT) return v.as.fval != 0.0;
    if (v.type == VAL_STR) return v.as.sval && v.as.sval[0] != '\0';
    if (v.type == VAL_LIST) return v.as.lval->count > 0;
    return true;
}

/* ============================================================================
 *  3. Scope / Environment
 * ============================================================================ */

static void scope_init(RdxScope *scope, RdxScope *parent) {
    scope->var_count = 0;
    scope->parent = parent;
}

static bool scope_set(RdxScope *scope, const char *name, RdxVal val) {
    /* Update existing */
    for (int i = 0; i < scope->var_count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) {
            scope->vars[i].val = val;
            return true;
        }
    }
    if (scope->parent && scope_set(scope->parent, name, val)) {
        return true;
    }
    /* Define in current scope */
    if (scope->var_count < 256) {
        strncpy(scope->vars[scope->var_count].name, name, 63);
        scope->vars[scope->var_count].name[63] = '\0';
        scope->vars[scope->var_count].val = val;
        scope->var_count++;
        return true;
    }
    return false;
}

static bool scope_get(RdxScope *scope, const char *name, RdxVal *out_val) {
    for (int i = 0; i < scope->var_count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) {
            *out_val = scope->vars[i].val;
            return true;
        }
    }
    if (scope->parent) {
        return scope_get(scope->parent, name, out_val);
    }
    return false;
}

/* ============================================================================
 *  4. Interpreter & Dispatch
 * ============================================================================ */

typedef struct {
    RdxToken   tokens[4096];
    int        token_count;
    int        current;
    const char *filename;
    RdxScope   global_scope;
    bool       had_error;
} RdxEngine;

static RdxToken *peek(RdxEngine *e) {
    return &e->tokens[e->current];
}

static RdxToken *previous(RdxEngine *e) {
    return &e->tokens[e->current - 1];
}

static bool is_at_end(RdxEngine *e) {
    return peek(e)->type == TOK_EOF;
}

static RdxToken *advance_tok(RdxEngine *e) {
    if (!is_at_end(e)) e->current++;
    return previous(e);
}

static bool check_tok(RdxEngine *e, TokenType type) {
    if (is_at_end(e)) return false;
    return peek(e)->type == type;
}

static bool match_tok(RdxEngine *e, TokenType type) {
    if (check_tok(e, type)) {
        advance_tok(e);
        return true;
    }
    return false;
}

static void skip_newlines(RdxEngine *e) {
    while (match_tok(e, TOK_NEWLINE) || match_tok(e, TOK_SEMICOLON));
}

/* Forward Declarations */
static RdxVal eval_expr(RdxEngine *e, RdxScope *scope);
static bool   exec_stmt(RdxEngine *e, RdxScope *scope, RdxVal *return_val);

/* Primary Expressions */
static RdxVal eval_primary(RdxEngine *e, RdxScope *scope) {
    skip_newlines(e);

    if (match_tok(e, TOK_INT)) {
        return val_int(previous(e)->ival);
    }
    if (match_tok(e, TOK_FLOAT)) {
        return val_float(previous(e)->fval);
    }
    if (match_tok(e, TOK_STR)) {
        return val_str(previous(e)->sval);
    }
    if (match_tok(e, TOK_TRUE)) {
        return val_bool(true);
    }
    if (match_tok(e, TOK_FALSE)) {
        return val_bool(false);
    }
    if (match_tok(e, TOK_NIL)) {
        return val_nil();
    }

    /* List Literal: [1, 2, "3"] */
    if (match_tok(e, TOK_LBRACKET)) {
        RdxVal list = val_list();
        skip_newlines(e);
        if (!check_tok(e, TOK_RBRACKET)) {
            do {
                skip_newlines(e);
                RdxVal elem = eval_expr(e, scope);
                list_append(list.as.lval, elem);
                skip_newlines(e);
            } while (match_tok(e, TOK_COMMA));
        }
        skip_newlines(e);
        if (!match_tok(e, TOK_RBRACKET)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR",
                            "Expected closing bracket ']' for list literal.",
                            "Add a closing bracket ']' to close the list.");
            e->had_error = true;
        }
        return list;
    }

    /* Grouping: (expr) */
    if (match_tok(e, TOK_LPAREN)) {
        RdxVal v = eval_expr(e, scope);
        if (!match_tok(e, TOK_RPAREN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR",
                            "Expected closing parenthesis ')' after expression.",
                            "Add ')' to balance your parenthesis.");
            e->had_error = true;
        }
        return v;
    }

    /* Identifier or Builtin Function Call */
    if (match_tok(e, TOK_IDENT)) {
        RdxToken *id_tok = previous(e);
        char name[128];
        strncpy(name, id_tok->sval, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        /* Is it a function or method call? e.g. name(...) */
        if (check_tok(e, TOK_LPAREN)) {
            advance_tok(e); /* consume '(' */
            RdxVal args[16];
            int arg_count = 0;
            skip_newlines(e);
            if (!check_tok(e, TOK_RPAREN)) {
                do {
                    skip_newlines(e);
                    if (arg_count < 16) {
                        args[arg_count++] = eval_expr(e, scope);
                    }
                    skip_newlines(e);
                } while (match_tok(e, TOK_COMMA));
            }
            if (!match_tok(e, TOK_RPAREN)) {
                rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                "SYNTAX ERROR",
                                "Missing closing parenthesis ')' in function call.",
                                "Close arguments with ')'.");
                e->had_error = true;
            }

            /* ── Cyber Builtins Dispatch ── */
            /* ip.is_valid */
            if (strcmp(name, "ip.is_valid") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
                return val_bool(rdx_ip_is_valid(args[0].as.sval));
            }
            /* ip.is_private */
            if (strcmp(name, "ip.is_private") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
                return val_bool(rdx_ip_is_private(args[0].as.sval));
            }
            /* ip.is_public */
            if (strcmp(name, "ip.is_public") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
                return val_bool(rdx_ip_is_public(args[0].as.sval));
            }
            /* ip.is_loopback */
            if (strcmp(name, "ip.is_loopback") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
                return val_bool(rdx_ip_is_loopback(args[0].as.sval));
            }
            /* ip.in_subnet */
            if (strcmp(name, "ip.in_subnet") == 0) {
                if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) {
                    rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                                    "ARGUMENT ERROR",
                                    "ip.in_subnet(ip, cidr) expects 2 string arguments.",
                                    "Example: ip.in_subnet(\"192.168.1.5\", \"192.168.1.0/24\")");
                    return val_bool(false);
                }
                return val_bool(rdx_ip_in_subnet(args[0].as.sval, args[1].as.sval));
            }
            /* ip.octets */
            if (strcmp(name, "ip.octets") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_nil();
                int octs[4];
                if (!rdx_ip_octets(args[0].as.sval, octs)) return val_nil();
                RdxVal l = val_list();
                for (int i = 0; i < 4; i++) list_append(l.as.lval, val_int(octs[i]));
                return l;
            }

            /* hash.md5 */
            if (strcmp(name, "hash.md5") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char out[33];
                rdx_hash_md5(args[0].as.sval, strlen(args[0].as.sval), out);
                return val_str(out);
            }
            /* hash.sha1 */
            if (strcmp(name, "hash.sha1") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char out[41];
                rdx_hash_sha1(args[0].as.sval, strlen(args[0].as.sval), out);
                return val_str(out);
            }
            /* hash.sha256 */
            if (strcmp(name, "hash.sha256") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char out[65];
                rdx_hash_sha256(args[0].as.sval, strlen(args[0].as.sval), out);
                return val_str(out);
            }
            /* hash.file_md5 */
            if (strcmp(name, "hash.file_md5") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char out[33];
                if (rdx_hash_file_md5(args[0].as.sval, out)) return val_str(out);
                return val_str("FILE_NOT_FOUND");
            }
            /* hash.file_sha256 */
            if (strcmp(name, "hash.file_sha256") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char out[65];
                if (rdx_hash_file_sha256(args[0].as.sval, out)) return val_str(out);
                return val_str("FILE_NOT_FOUND");
            }
            /* hash.match */
            if (strcmp(name, "hash.match") == 0) {
                if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) return val_bool(false);
                return val_bool(rdx_hash_match(args[0].as.sval, args[1].as.sval));
            }

            /* net.port_name */
            if (strcmp(name, "net.port_name") == 0) {
                int p = (arg_count > 0 && args[0].type == VAL_INT) ? (int)args[0].as.ival : 0;
                return val_str(rdx_net_port_name(p));
            }
            /* net.port_risk */
            if (strcmp(name, "net.port_risk") == 0) {
                int p = (arg_count > 0 && args[0].type == VAL_INT) ? (int)args[0].as.ival : 0;
                return val_str(rdx_net_port_risk(p));
            }

            /* log.extract_ip */
            if (strcmp(name, "log.extract_ip") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char ip_out[64];
                if (rdx_log_extract_ip(args[0].as.sval, ip_out)) return val_str(ip_out);
                return val_str("");
            }
            /* log.extract_url */
            if (strcmp(name, "log.extract_url") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char url_out[256];
                if (rdx_log_extract_url(args[0].as.sval, url_out)) return val_str(url_out);
                return val_str("");
            }
            /* log.extract_cve */
            if (strcmp(name, "log.extract_cve") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                char cve_out[64];
                if (rdx_log_extract_cve(args[0].as.sval, cve_out)) return val_str(cve_out);
                return val_str("");
            }
            /* log.extract_status */
            if (strcmp(name, "log.extract_status") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_int(0);
                return val_int(rdx_log_extract_status(args[0].as.sval));
            }
            /* log.detect_attack */
            if (strcmp(name, "log.detect_attack") == 0) {
                if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
                const char *atk = rdx_log_detect_attack(args[0].as.sval);
                return val_str(atk ? atk : "");
            }

            /* threat.is_ioc */
            if (strcmp(name, "threat.is_ioc") == 0) {
                if (arg_count >= 2 && args[0].type == VAL_STR && args[1].type == VAL_LIST) {
                    for (int i = 0; i < args[1].as.lval->count; i++) {
                        if (args[1].as.lval->items[i].type == VAL_STR &&
                            rdx_hash_match(args[0].as.sval, args[1].as.lval->items[i].as.sval)) {
                            return val_bool(true);
                        }
                    }
                }
                return val_bool(false);
            }

            /* Utility functions */
            if (strcmp(name, "len") == 0) {
                if (arg_count > 0 && args[0].type == VAL_LIST) return val_int(args[0].as.lval->count);
                if (arg_count > 0 && args[0].type == VAL_STR) return val_int(strlen(args[0].as.sval));
                return val_int(0);
            }
            if (strcmp(name, "range") == 0) {
                int start = 0, end = 0;
                if (arg_count == 1 && args[0].type == VAL_INT) {
                    end = (int)args[0].as.ival;
                } else if (arg_count >= 2 && args[0].type == VAL_INT && args[1].type == VAL_INT) {
                    start = (int)args[0].as.ival;
                    end = (int)args[1].as.ival;
                }
                RdxVal l = val_list();
                for (int i = start; i < end; i++) list_append(l.as.lval, val_int(i));
                return l;
            }

            /* Fuzzy Suggestion for Typo in Function Call */
            static const char *BUILTIN_FUNCS[] = {
                "ip.is_valid", "ip.is_private", "ip.is_public", "ip.is_loopback", "ip.in_subnet", "ip.octets",
                "hash.md5", "hash.sha1", "hash.sha256", "hash.file_md5", "hash.file_sha256", "hash.match",
                "net.port_name", "net.port_risk",
                "log.extract_ip", "log.extract_url", "log.extract_cve", "log.extract_status", "log.detect_attack",
                "threat.is_ioc", "alert", "timeline", "len", "range"
            };
            const char *cand = rdx_diag_suggest_similar(name, BUILTIN_FUNCS, sizeof(BUILTIN_FUNCS)/sizeof(char*));
            char why_buf[256];
            snprintf(why_buf, sizeof(why_buf), "Unknown function or procedure '%s'.", name);
            char fix_buf[256];
            if (cand) {
                snprintf(fix_buf, sizeof(fix_buf), "Did you mean '%s'? Check spelling and try again.", cand);
            } else {
                snprintf(fix_buf, sizeof(fix_buf), "Verify function name or define it with: fn %s(...) { ... }", name);
            }
            rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                            "NAME ERROR", why_buf, fix_buf);
            e->had_error = true;
            return val_nil();
        }

        /* Variable lookup */
        RdxVal v;
        if (scope_get(scope, name, &v)) {
            /* Optional list indexing: var[idx] */
            if (match_tok(e, TOK_LBRACKET)) {
                RdxVal idx = eval_expr(e, scope);
                if (!match_tok(e, TOK_RBRACKET)) {
                    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                    "SYNTAX ERROR", "Missing closing bracket ']' in index.", "Add ']' to close index.");
                    e->had_error = true;
                }
                if (v.type == VAL_LIST && idx.type == VAL_INT) {
                    int i = (int)idx.as.ival;
                    if (i >= 0 && i < v.as.lval->count) {
                        return v.as.lval->items[i];
                    } else {
                        char why[128];
                        snprintf(why, sizeof(why), "Index %d out of bounds for list of length %d.", i, v.as.lval->count);
                        rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                                        "INDEX ERROR", why, "Ensure list index is between 0 and len(list)-1.");
                        return val_nil();
                    }
                }
            }
            return v;
        }

        /* Undefined variable with fuzzy check */
        const char *known_vars[256];
        int kcount = 0;
        for (int i = 0; i < scope->var_count && kcount < 256; i++) {
            known_vars[kcount++] = scope->vars[i].name;
        }
        const char *cand = rdx_diag_suggest_similar(name, known_vars, kcount);
        char why_buf[256];
        snprintf(why_buf, sizeof(why_buf), "Variable '%s' has not been defined.", name);
        char fix_buf[256];
        if (cand) {
            snprintf(fix_buf, sizeof(fix_buf), "Did you mean '%s'? Or assign it first with: %s = ...", cand, name);
        } else {
            snprintf(fix_buf, sizeof(fix_buf), "Define it before reading it, e.g.: let %s = \"value\"", name);
        }
        rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                        "UNDEFINED VARIABLE", why_buf, fix_buf);
        e->had_error = true;
        return val_nil();
    }

    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                    "SYNTAX ERROR",
                    "Unexpected token in expression.",
                    "Check for missing operators, variables, or unclosed quotes.");
    e->had_error = true;
    advance_tok(e);
    return val_nil();
}

/* Binary Operations and Expressions */
static RdxVal eval_binary(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_primary(e, scope);

    while (match_tok(e, TOK_PLUS) || match_tok(e, TOK_MINUS) ||
           match_tok(e, TOK_STAR) || match_tok(e, TOK_SLASH) ||
           match_tok(e, TOK_PERCENT) || match_tok(e, TOK_EQ) ||
           match_tok(e, TOK_NEQ) || match_tok(e, TOK_AND) ||
           match_tok(e, TOK_OR) || match_tok(e, TOK_LT) ||
           match_tok(e, TOK_LTE) || match_tok(e, TOK_GT) ||
           match_tok(e, TOK_GTE)) {
        TokenType op = previous(e)->type;
        RdxToken *op_tok = previous(e);
        RdxVal right = eval_primary(e, scope);

        if (op == TOK_PLUS) {
            if (left.type == VAL_INT && right.type == VAL_INT) {
                left = val_int(left.as.ival + right.as.ival);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = (left.type == VAL_FLOAT) ? left.as.fval : (double)left.as.ival;
                double r = (right.type == VAL_FLOAT) ? right.as.fval : (double)right.as.ival;
                left = val_float(l + r);
            } else if (left.type == VAL_STR || right.type == VAL_STR) {
                char buf[1024];
                char lbuf[512], rbuf[512];
                if (left.type == VAL_STR) strncpy(lbuf, left.as.sval, 511);
                else snprintf(lbuf, 511, "%lld", (long long)left.as.ival);
                if (right.type == VAL_STR) strncpy(rbuf, right.as.sval, 511);
                else snprintf(rbuf, 511, "%lld", (long long)right.as.ival);
                snprintf(buf, sizeof(buf), "%s%s", lbuf, rbuf);
                left = val_str(buf);
            }
        } else if (op == TOK_MINUS) {
            left = val_int(left.as.ival - right.as.ival);
        } else if (op == TOK_STAR) {
            left = val_int(left.as.ival * right.as.ival);
        } else if (op == TOK_SLASH) {
            if (right.as.ival == 0) {
                rdx_diag_report(e->filename, op_tok->line, op_tok->col, 1,
                                "RUNTIME ERROR",
                                "Division by zero.",
                                "Ensure the denominator is non-zero before dividing.");
                e->had_error = true;
                return val_int(0);
            }
            left = val_int(left.as.ival / right.as.ival);
        } else if (op == TOK_PERCENT) {
            if (right.as.ival != 0) left = val_int(left.as.ival % right.as.ival);
        } else if (op == TOK_EQ) {
            if (left.type == VAL_STR && right.type == VAL_STR) {
                left = val_bool(strcmp(left.as.sval, right.as.sval) == 0);
            } else if (left.type == VAL_INT && right.type == VAL_INT) {
                left = val_bool(left.as.ival == right.as.ival);
            } else if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                left = val_bool(left.as.bval == right.as.bval);
            } else {
                left = val_bool(false);
            }
        } else if (op == TOK_NEQ) {
            if (left.type == VAL_STR && right.type == VAL_STR) {
                left = val_bool(strcmp(left.as.sval, right.as.sval) != 0);
            } else {
                left = val_bool(left.as.ival != right.as.ival);
            }
        } else if (op == TOK_LT) {
            left = val_bool(left.as.ival < right.as.ival);
        } else if (op == TOK_LTE) {
            left = val_bool(left.as.ival <= right.as.ival);
        } else if (op == TOK_GT) {
            left = val_bool(left.as.ival > right.as.ival);
        } else if (op == TOK_GTE) {
            left = val_bool(left.as.ival >= right.as.ival);
        } else if (op == TOK_AND) {
            left = val_bool(val_is_truthy(left) && val_is_truthy(right));
        } else if (op == TOK_OR) {
            left = val_bool(val_is_truthy(left) || val_is_truthy(right));
        }
    }
    return left;
}

static RdxVal eval_expr(RdxEngine *e, RdxScope *scope) {
    return eval_binary(e, scope);
}

/* Statements Execution */
static bool exec_stmt(RdxEngine *e, RdxScope *scope, RdxVal *return_val) {
    skip_newlines(e);
    if (is_at_end(e)) return false;

    /* print / println statement */
    if (match_tok(e, TOK_PRINT)) {
        bool first = true;
        /* Can be print expr or print(expr, ...) */
        bool paren = match_tok(e, TOK_LPAREN);
        do {
            skip_newlines(e);
            RdxVal v = eval_expr(e, scope);
            if (!first) printf(" ");
            val_print(v);
            first = false;
            skip_newlines(e);
        } while (match_tok(e, TOK_COMMA));

        if (paren) match_tok(e, TOK_RPAREN);
        printf("\n");
        return true;
    }

    /* alert statement: alert "CRITICAL" "Message" "Details" */
    if (match_tok(e, TOK_ALERT)) {
        bool paren = match_tok(e, TOK_LPAREN);
        RdxVal lvl = eval_expr(e, scope);
        RdxVal msg = val_str("");
        RdxVal det = val_str("");
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) || (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            msg = eval_expr(e, scope);
        }
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) || (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            det = eval_expr(e, scope);
        }
        if (paren) match_tok(e, TOK_RPAREN);
        rdx_cyber_alert(lvl.type == VAL_STR ? lvl.as.sval : "ALERT",
                        msg.type == VAL_STR ? msg.as.sval : "",
                        det.type == VAL_STR ? det.as.sval : "");
        return true;
    }

    /* timeline statement: timeline "EVENT" "Details" */
    if (match_tok(e, TOK_TIMELINE)) {
        bool paren = match_tok(e, TOK_LPAREN);
        RdxVal evt = eval_expr(e, scope);
        RdxVal det = val_str("");
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) || (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            det = eval_expr(e, scope);
        }
        if (paren) match_tok(e, TOK_RPAREN);
        rdx_cyber_timeline(evt.type == VAL_STR ? evt.as.sval : "EVENT",
                           det.type == VAL_STR ? det.as.sval : "");
        return true;
    }

    /* let variable = expr */
    if (match_tok(e, TOK_LET)) {
        if (!match_tok(e, TOK_IDENT)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR",
                            "Expected variable name after 'let'.",
                            "Provide an identifier: let target = \"10.0.0.1\"");
            e->had_error = true;
            return false;
        }
        char varname[64];
        strncpy(varname, previous(e)->sval, 63);
        varname[63] = '\0';

        if (!match_tok(e, TOK_ASSIGN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR",
                            "Expected '=' assignment after variable declaration.",
                            "Assign a value: let x = 42");
            e->had_error = true;
            return false;
        }
        RdxVal val = eval_expr(e, scope);
        scope_set(scope, varname, val);
        return true;
    }

    /* if condition { ... } else { ... } */
    if (match_tok(e, TOK_IF)) {
        RdxVal cond = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR",
                            "Expected opening brace '{' after if condition.",
                            "Use '{' to start the block, e.g.: if condition { ... }");
            e->had_error = true;
            return false;
        }

        bool truthy = val_is_truthy(cond);
        if (truthy) {
            while (!check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
                exec_stmt(e, scope, return_val);
                skip_newlines(e);
            }
            if (!match_tok(e, TOK_RBRACE)) {
                rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                "SYNTAX ERROR", "Unclosed block in 'if' body.", "Add '}' to close the if block.");
                e->had_error = true;
            }
            /* Skip else if present */
            skip_newlines(e);
            if (match_tok(e, TOK_ELSE)) {
                skip_newlines(e);
                if (match_tok(e, TOK_LBRACE)) {
                    int depth = 1;
                    while (depth > 0 && !is_at_end(e)) {
                        if (match_tok(e, TOK_LBRACE)) depth++;
                        else if (match_tok(e, TOK_RBRACE)) depth--;
                        else advance_tok(e);
                    }
                }
            }
        } else {
            /* Skip then-block */
            int depth = 1;
            while (depth > 0 && !is_at_end(e)) {
                if (match_tok(e, TOK_LBRACE)) depth++;
                else if (match_tok(e, TOK_RBRACE)) depth--;
                else advance_tok(e);
            }
            skip_newlines(e);
            if (match_tok(e, TOK_ELSE)) {
                skip_newlines(e);
                if (match_tok(e, TOK_LBRACE)) {
                    while (!check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
                        exec_stmt(e, scope, return_val);
                        skip_newlines(e);
                    }
                    if (!match_tok(e, TOK_RBRACE)) {
                        rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                        "SYNTAX ERROR", "Unclosed block in 'else' body.", "Add '}' to close the else block.");
                        e->had_error = true;
                    }
                }
            }
        }
        return true;
    }

    /* while condition { ... } */
    if (match_tok(e, TOK_WHILE)) {
        int cond_pos = e->current;
        RdxVal cond = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' after while condition.", "Add '{' to start the loop body.");
            e->had_error = true;
            return false;
        }
        int body_pos = e->current;

        while (val_is_truthy(cond)) {
            e->current = body_pos;
            while (!check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
                exec_stmt(e, scope, return_val);
                skip_newlines(e);
            }
            /* Re-evaluate condition */
            e->current = cond_pos;
            cond = eval_expr(e, scope);
            skip_newlines(e);
            match_tok(e, TOK_LBRACE);
        }

        /* Skip past body */
        int depth = 1;
        while (depth > 0 && !is_at_end(e)) {
            if (match_tok(e, TOK_LBRACE)) depth++;
            else if (match_tok(e, TOK_RBRACE)) depth--;
            else advance_tok(e);
        }
        return true;
    }

    /* for item in list { ... } */
    if (match_tok(e, TOK_FOR)) {
        if (!match_tok(e, TOK_IDENT)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected loop variable name after 'for'.", "Example: for ip in ip_list { ... }");
            e->had_error = true;
            return false;
        }
        char loop_var[64];
        strncpy(loop_var, previous(e)->sval, 63);
        loop_var[63] = '\0';

        if (!match_tok(e, TOK_IN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected 'in' keyword in for loop.", "Example: for x in list { ... }");
            e->had_error = true;
            return false;
        }

        RdxVal iter = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' after for collection.", "Add '{' to start the loop body.");
            e->had_error = true;
            return false;
        }
        int body_start = e->current;

        if (iter.type == VAL_LIST) {
            for (int i = 0; i < iter.as.lval->count; i++) {
                scope_set(scope, loop_var, iter.as.lval->items[i]);
                e->current = body_start;
                while (!check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
                    exec_stmt(e, scope, return_val);
                    skip_newlines(e);
                }
            }
        }

        /* Skip past body */
        e->current = body_start;
        int depth = 1;
        while (depth > 0 && !is_at_end(e)) {
            if (match_tok(e, TOK_LBRACE)) depth++;
            else if (match_tok(e, TOK_RBRACE)) depth--;
            else advance_tok(e);
        }
        return true;
    }

    /* Top-level variable assignment without 'let': name = expr */
    if (check_tok(e, TOK_IDENT) && e->tokens[e->current + 1].type == TOK_ASSIGN) {
        advance_tok(e); /* ident */
        char varname[64];
        strncpy(varname, previous(e)->sval, 63);
        varname[63] = '\0';
        advance_tok(e); /* '=' */
        RdxVal val = eval_expr(e, scope);
        scope_set(scope, varname, val);
        return true;
    }

    /* Fallback: Expression statement (e.g. function call or expression) */
    eval_expr(e, scope);
    return true;
}

/* ============================================================================
 *  5. Engine Run & REPL
 * ============================================================================ */

int rdx_engine_run(const char *filename, const char *source) {
    if (!source) return 1;

    rdx_diag_set_context(filename, source, strlen(source));

    RdxEngine *eng = (RdxEngine*)calloc(1, sizeof(RdxEngine));
    if (!eng) return 1;
    eng->filename = filename ? filename : "<script>";
    scope_init(&eng->global_scope, NULL);

    /* Tokenize */
    RdxLexer lex;
    lexer_init(&lex, filename, source);
    while (eng->token_count < 4095) {
        RdxToken tok = lexer_next_token(&lex);
        eng->tokens[eng->token_count++] = tok;
        if (tok.type == TOK_EOF) break;
    }

    /* Execute all statements */
    while (!is_at_end(eng) && !eng->had_error) {
        skip_newlines(eng);
        if (is_at_end(eng)) break;
        exec_stmt(eng, &eng->global_scope, NULL);
        skip_newlines(eng);
    }

    int ret = eng->had_error ? 1 : 0;
    free(eng);
    return ret;
}

void rdx_engine_repl(void) {
    printf("\n");
    printf("\033[1;36m+==============================================================+\033[0m\n");
    printf("\033[1;36m|  RDX Forensics & Cyber Security Language v2.5 [Standalone]    |\033[0m\n");
    printf("\033[1;36m|  Syntax-Free * Zero-Friction * Built-in Threat & Net Engine   |\033[0m\n");
    printf("\033[1;36m+==============================================================+\033[0m\n");
    printf("Type 'help' for cybersecurity commands, or 'exit' to quit.\n\n");

    RdxEngine *eng = (RdxEngine*)calloc(1, sizeof(RdxEngine));
    if (!eng) return;
    eng->filename = "<repl>";
    scope_init(&eng->global_scope, NULL);

    char line_buf[1024];
    while (1) {
        printf("\033[1;32mrdx>\033[0m ");
        fflush(stdout);
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;

        /* Strip trailing newline */
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }
        if (len == 0) continue;

        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "quit") == 0) {
            printf("Exiting RDX Forensics Shell.\n");
            break;
        }

        if (strcmp(line_buf, "help") == 0) {
            printf("\n--- RDX Cyber Forensics Built-in Modules ---\n");
            printf("  IP & CIDR:  ip.is_valid(ip), ip.is_private(ip), ip.is_public(ip)\n");
            printf("              ip.in_subnet(ip, cidr), ip.octets(ip)\n");
            printf("  Crypto:     hash.md5(str), hash.sha1(str), hash.sha256(str)\n");
            printf("              hash.file_md5(path), hash.file_sha256(path)\n");
            printf("  Network:    net.port_name(port), net.port_risk(port)\n");
            printf("  Log Audit:  log.extract_ip(line), log.extract_url(line), log.extract_cve(line)\n");
            printf("              log.detect_attack(line), log.extract_status(line)\n");
            printf("  Alerts:     alert(severity, summary, details)\n");
            printf("              timeline(event_name, details)\n");
            printf("  Syntax:     x = 42; if ip.is_private(x) { ... }; for item in list { ... }\n\n");
            continue;
        }

        rdx_diag_set_context("<repl>", line_buf, len);
        RdxLexer lex;
        lexer_init(&lex, "<repl>", line_buf);
        eng->token_count = 0;
        eng->current = 0;
        eng->had_error = false;

        while (eng->token_count < 4095) {
            RdxToken tok = lexer_next_token(&lex);
            eng->tokens[eng->token_count++] = tok;
            if (tok.type == TOK_EOF) break;
        }

        while (!is_at_end(eng) && !eng->had_error) {
            skip_newlines(eng);
            if (is_at_end(eng)) break;
            exec_stmt(eng, &eng->global_scope, NULL);
            skip_newlines(eng);
        }
    }
    free(eng);
}
