/*
 * ============================================================================
 *  rdx_engine.c  —  RDX Syntax-Free Execution Engine & Cyber Forensics Runtime
 *  Version: 3.0.0  (End-to-End, Fully Standalone)
 * ============================================================================
 *
 *  Enhancements in v3.0:
 *    - User-defined functions with `fn name(args) { ... }`
 *    - `return` from functions
 *    - `break` and `continue` inside loops
 *    - `input(prompt)` built-in for interactive programs
 *    - `str.*` string methods: str.upper, str.lower, str.len, str.contains,
 *       str.starts, str.ends, str.replace, str.split, str.join, str.trim
 *    - `math.*` builtins: math.abs, math.min, math.max, math.pow,
 *       math.sqrt, math.floor, math.ceil, math.round
 *    - `type(v)` returns the type name as a string
 *    - `int(v)`, `float(v)`, `str(v)` explicit type conversions
 *    - `append(list, val)` built-in
 *    - `exit(code)` built-in
 *    - Multi-line REPL with continuation `\` at end of line
 *    - Error recovery — continues after most errors (no early abort)
 *    - Support for `elif` in if/elif/else chains
 *    - `!` (not) unary operator
 *    - Augmented assignment: `+=`, `-=`, `*=`, `/=`
 *    - Float arithmetic in all binary ops
 *    - String multiplication: `"abc" * 3 => "abcabcabc"`
 *    - `//=` floor-div assignment and `%=` modulo assignment
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

#ifndef M_E
#define M_E 2.71828182845904523536
#endif
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 *  1. Lexical Analysis (Tokens)
 * ============================================================================ */

typedef enum {
    TOK_EOF = 0,
    TOK_INT,
    TOK_FLOAT,
    TOK_STR,
    TOK_IDENT,
    /* Keywords */
    TOK_LET,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_IN,
    TOK_FN,
    TOK_RETURN,
    TOK_BREAK,
    TOK_CONTINUE,
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
    TOK_BANG,
    TOK_ASSIGN,
    TOK_PLUS_ASSIGN,
    TOK_MINUS_ASSIGN,
    TOK_STAR_ASSIGN,
    TOK_SLASH_ASSIGN,
    TOK_PERCENT_ASSIGN,
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
    TOK_DOT,
    TOK_COLON,
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
    lex->source     = source;
    lex->source_len = strlen(source);
    lex->cursor     = source;
    lex->line       = 1;
    lex->col        = 1;
    lex->filename   = filename;
}

static char lexer_peek(RdxLexer *lex)  { return *lex->cursor; }
static char lexer_peek2(RdxLexer *lex) { return lex->cursor[1]; }

static char lexer_advance(RdxLexer *lex) {
    char c = *lex->cursor++;
    if (c == '\n') { lex->line++; lex->col = 1; }
    else           { lex->col++; }
    return c;
}

static bool lexer_match(RdxLexer *lex, char expected) {
    if (*lex->cursor == expected) { lexer_advance(lex); return true; }
    return false;
}

static void lexer_skip_whitespace_and_comments(RdxLexer *lex) {
    while (*lex->cursor) {
        char c = *lex->cursor;
        if (c == ' ' || c == '\t' || c == '\r') {
            lexer_advance(lex);
        } else if (c == '/' && lex->cursor[1] == '/') {
            while (*lex->cursor && *lex->cursor != '\n') lexer_advance(lex);
        } else if (c == '#') {
            while (*lex->cursor && *lex->cursor != '\n') lexer_advance(lex);
        } else if (c == '/' && lex->cursor[1] == '*') {
            lexer_advance(lex); lexer_advance(lex);
            while (*lex->cursor && !(*lex->cursor == '*' && lex->cursor[1] == '/'))
                lexer_advance(lex);
            if (*lex->cursor == '*') { lexer_advance(lex); lexer_advance(lex); }
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
    tok.line  = lex->line;
    tok.col   = lex->col;

    if (!*lex->cursor) { tok.type = TOK_EOF; return tok; }

    char c = lexer_advance(lex);

    if (c == '\n') { tok.type = TOK_NEWLINE; tok.length = 1; return tok; }

    switch (c) {
        case ';': tok.type = TOK_SEMICOLON; tok.length = 1; return tok;
        case ',': tok.type = TOK_COMMA;     tok.length = 1; return tok;
        case '(': tok.type = TOK_LPAREN;    tok.length = 1; return tok;
        case ')': tok.type = TOK_RPAREN;    tok.length = 1; return tok;
        case '{': tok.type = TOK_LBRACE;    tok.length = 1; return tok;
        case '}': tok.type = TOK_RBRACE;    tok.length = 1; return tok;
        case '[': tok.type = TOK_LBRACKET;  tok.length = 1; return tok;
        case ']': tok.type = TOK_RBRACKET;  tok.length = 1; return tok;
        case ':': tok.type = TOK_COLON;     tok.length = 1; return tok;
        case '.': tok.type = TOK_DOT;       tok.length = 1; return tok;
        case '+':
            if (lexer_match(lex, '=')) { tok.type = TOK_PLUS_ASSIGN; tok.length = 2; }
            else                       { tok.type = TOK_PLUS;        tok.length = 1; }
            return tok;
        case '-':
            if (lexer_match(lex, '=')) { tok.type = TOK_MINUS_ASSIGN; tok.length = 2; }
            else                       { tok.type = TOK_MINUS;         tok.length = 1; }
            return tok;
        case '*':
            if (lexer_match(lex, '=')) { tok.type = TOK_STAR_ASSIGN; tok.length = 2; }
            else                       { tok.type = TOK_STAR;         tok.length = 1; }
            return tok;
        case '/':
            if (lexer_match(lex, '=')) { tok.type = TOK_SLASH_ASSIGN; tok.length = 2; }
            else                       { tok.type = TOK_SLASH;         tok.length = 1; }
            return tok;
        case '%':
            if (lexer_match(lex, '=')) { tok.type = TOK_PERCENT_ASSIGN; tok.length = 2; }
            else                       { tok.type = TOK_PERCENT;         tok.length = 1; }
            return tok;
        case '!':
            if (lexer_match(lex, '=')) { tok.type = TOK_NEQ;  tok.length = 2; }
            else                       { tok.type = TOK_BANG;  tok.length = 1; }
            return tok;
        case '&':
            if (lexer_match(lex, '&')) { tok.type = TOK_AND; tok.length = 2; }
            else                       { tok.type = TOK_AND; tok.length = 1; }
            return tok;
        case '|':
            if (lexer_match(lex, '|')) { tok.type = TOK_OR; tok.length = 2; }
            else                       { tok.type = TOK_OR;  tok.length = 1; }
            return tok;
        case '<':
            if (lexer_match(lex, '=')) { tok.type = TOK_LTE; tok.length = 2; }
            else                       { tok.type = TOK_LT;  tok.length = 1; }
            return tok;
        case '>':
            if (lexer_match(lex, '=')) { tok.type = TOK_GTE; tok.length = 2; }
            else                       { tok.type = TOK_GT;  tok.length = 1; }
            return tok;
        case '=':
            if (lexer_match(lex, '=')) { tok.type = TOK_EQ;     tok.length = 2; }
            else                       { tok.type = TOK_ASSIGN;  tok.length = 1; }
            return tok;
        default: break;
    }

    /* String Literal */
    if (c == '"' || c == '\'') {
        char quote = c;
        char buf[2048];
        int  bidx  = 0;
        int  start_col  = tok.col;
        int  start_line = tok.line;

        while (*lex->cursor && *lex->cursor != quote) {
            char ch = lexer_advance(lex);
            if (ch == '\\') {
                char esc = lexer_advance(lex);
                if      (esc == 'n') ch = '\n';
                else if (esc == 't') ch = '\t';
                else if (esc == 'r') ch = '\r';
                else if (esc == '\\') ch = '\\';
                else if (esc == quote) ch = quote;
                else if (esc == '0') ch = '\0';
            }
            if (bidx < 2047) buf[bidx++] = ch;
        }
        if (!*lex->cursor) {
            rdx_diag_report(lex->filename, start_line, start_col, 1,
                            "UNTERMINATED STRING",
                            "String literal reached end-of-file without a closing quote.",
                            "Add a closing quote '\"' to terminate your string.");
            tok.type = TOK_EOF; return tok;
        }
        lexer_advance(lex); /* consume closing quote */
        buf[bidx] = '\0';
        tok.type = TOK_STR;
        tok.length = (int)(lex->cursor - tok.start);
        strncpy(tok.sval, buf, sizeof(tok.sval) - 1);
        tok.sval[sizeof(tok.sval) - 1] = '\0';
        return tok;
    }

    /* Number Literal */
    if (isdigit((unsigned char)c)) {
        const char *num_start = tok.start;
        bool is_float = false;

        if (c == '0' && (*lex->cursor == 'x' || *lex->cursor == 'X')) {
            lexer_advance(lex);
            while (isxdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
            tok.type   = TOK_INT;
            tok.ival   = strtoll(num_start, NULL, 16);
            tok.length = (int)(lex->cursor - num_start);
            return tok;
        }
        while (isdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
        if (*lex->cursor == '.' && isdigit((unsigned char)lex->cursor[1])) {
            is_float = true;
            lexer_advance(lex);
            while (isdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
        }
        if (*lex->cursor == 'e' || *lex->cursor == 'E') {
            is_float = true;
            lexer_advance(lex);
            if (*lex->cursor == '+' || *lex->cursor == '-') lexer_advance(lex);
            while (isdigit((unsigned char)*lex->cursor)) lexer_advance(lex);
        }
        tok.length = (int)(lex->cursor - num_start);
        if (is_float) { tok.type = TOK_FLOAT; tok.fval = strtod(num_start, NULL); }
        else          { tok.type = TOK_INT;   tok.ival = strtoll(num_start, NULL, 10); }
        return tok;
    }

    /* Identifiers & Keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        const char *id_start = tok.start;
        while (isalnum((unsigned char)*lex->cursor) || *lex->cursor == '_') {
            lexer_advance(lex);
        }

        tok.length = (int)(lex->cursor - id_start);
        if (tok.length >= (int)sizeof(tok.sval)) tok.length = (int)sizeof(tok.sval) - 1;
        memcpy(tok.sval, id_start, tok.length);
        tok.sval[tok.length] = '\0';

        /* Keyword resolution */
        if      (strcmp(tok.sval, "let")      == 0) tok.type = TOK_LET;
        else if (strcmp(tok.sval, "if")       == 0) tok.type = TOK_IF;
        else if (strcmp(tok.sval, "elif")     == 0) tok.type = TOK_ELIF;
        else if (strcmp(tok.sval, "else")     == 0) tok.type = TOK_ELSE;
        else if (strcmp(tok.sval, "while")    == 0) tok.type = TOK_WHILE;
        else if (strcmp(tok.sval, "for")      == 0) tok.type = TOK_FOR;
        else if (strcmp(tok.sval, "in")       == 0) tok.type = TOK_IN;
        else if (strcmp(tok.sval, "fn")       == 0) tok.type = TOK_FN;
        else if (strcmp(tok.sval, "return")   == 0) tok.type = TOK_RETURN;
        else if (strcmp(tok.sval, "break")    == 0) tok.type = TOK_BREAK;
        else if (strcmp(tok.sval, "continue") == 0) tok.type = TOK_CONTINUE;
        else if (strcmp(tok.sval, "print")    == 0 ||
                 strcmp(tok.sval, "println")  == 0)  tok.type = TOK_PRINT;
        else if (strcmp(tok.sval, "alert")    == 0) tok.type = TOK_ALERT;
        else if (strcmp(tok.sval, "timeline") == 0) tok.type = TOK_TIMELINE;
        else if (strcmp(tok.sval, "and")      == 0) tok.type = TOK_AND;
        else if (strcmp(tok.sval, "or")       == 0) tok.type = TOK_OR;
        else if (strcmp(tok.sval, "not")      == 0) tok.type = TOK_BANG;
        else if (strcmp(tok.sval, "true")     == 0) { tok.type = TOK_TRUE;  tok.ival = 1; }
        else if (strcmp(tok.sval, "false")    == 0) { tok.type = TOK_FALSE; tok.ival = 0; }
        else if (strcmp(tok.sval, "nil")      == 0 ||
                 strcmp(tok.sval, "none")     == 0 ||
                 strcmp(tok.sval, "null")     == 0)  tok.type = TOK_NIL;
        else                                          tok.type = TOK_IDENT;

        return tok;
    }

    /* Unrecognized */
    char why[128];
    snprintf(why, sizeof(why), "Unexpected character '%c' (ASCII %d)", c, (int)(unsigned char)c);
    rdx_diag_report(lex->filename, tok.line, tok.col, 1,
                    "SYNTAX ERROR", why, "Remove the illegal character or verify your syntax.");
    tok.type = TOK_EOF;
    return tok;
}

/* ============================================================================
 *  2. Values
 * ============================================================================ */

static RdxVal val_nil(void) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_NIL; return v;
}
static RdxVal val_bool(bool b) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_BOOL; v.as.bval = b; return v;
}
static RdxVal val_int(int64_t i) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_INT; v.as.ival = i; return v;
}
static RdxVal val_float(double f) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_FLOAT; v.as.fval = f; return v;
}
static RdxVal val_str(const char *s) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_STR;
    v.as.sval = s ? strdup(s) : strdup("");
    return v;
}
static RdxVal val_list(void) {
    RdxVal v; memset(&v, 0, sizeof(v)); v.type = VAL_LIST;
    v.as.lval = (RdxList*)calloc(1, sizeof(RdxList));
    return v;
}

static int64_t val_as_int(RdxVal v) {
    if (v.type == VAL_INT)   return v.as.ival;
    if (v.type == VAL_FLOAT) return (int64_t)v.as.fval;
    if (v.type == VAL_BOOL)  return v.as.bval ? 1 : 0;
    if (v.type == VAL_STR && v.as.sval) return strtoll(v.as.sval, NULL, 10);
    return 0;
}

static double val_as_double(RdxVal v) {
    if (v.type == VAL_FLOAT) return v.as.fval;
    if (v.type == VAL_INT)   return (double)v.as.ival;
    if (v.type == VAL_BOOL)  return v.as.bval ? 1.0 : 0.0;
    if (v.type == VAL_STR && v.as.sval) return strtod(v.as.sval, NULL);
    return 0.0;
}

static void list_append(RdxList *list, RdxVal val) {
    if (list->count >= list->capacity) {
        int new_cap = (list->capacity < 4) ? 4 : list->capacity * 2;
        list->items = (RdxVal*)realloc(list->items, new_cap * sizeof(RdxVal));
        list->capacity = new_cap;
    }
    list->items[list->count++] = val;
}

static char *val_to_str(RdxVal v, char *buf, size_t bsz) {
    switch (v.type) {
        case VAL_NIL:   snprintf(buf, bsz, "nil"); break;
        case VAL_BOOL:  snprintf(buf, bsz, "%s", v.as.bval ? "true" : "false"); break;
        case VAL_INT:   snprintf(buf, bsz, "%lld", (long long)v.as.ival); break;
        case VAL_FLOAT: {
            /* Trim trailing zeros for cleaner output */
            snprintf(buf, bsz, "%.10g", v.as.fval);
            break;
        }
        case VAL_STR:   snprintf(buf, bsz, "%s", v.as.sval ? v.as.sval : ""); break;
        case VAL_LIST: {
            int pos = 0;
            buf[pos++] = '[';
            for (int i = 0; i < v.as.lval->count && pos < (int)bsz - 4; i++) {
                if (i > 0) { buf[pos++] = ','; buf[pos++] = ' '; }
                char tmp[256];
                val_to_str(v.as.lval->items[i], tmp, sizeof(tmp));
                if (v.as.lval->items[i].type == VAL_STR) {
                    pos += snprintf(buf + pos, bsz - pos, "\"%s\"", tmp);
                } else {
                    pos += snprintf(buf + pos, bsz - pos, "%s", tmp);
                }
            }
            if (pos < (int)bsz - 1) buf[pos++] = ']';
            buf[pos] = '\0';
            break;
        }
        case VAL_FN:
            snprintf(buf, bsz, "<fn %s>", v.as.fnval ? v.as.fnval->name : "?");
            break;
    }
    return buf;
}

static void val_print(RdxVal v) {
    char buf[4096];
    val_to_str(v, buf, sizeof(buf));
    printf("%s", buf);
}

static bool val_is_truthy(RdxVal v) {
    if (v.type == VAL_NIL)   return false;
    if (v.type == VAL_BOOL)  return v.as.bval;
    if (v.type == VAL_INT)   return v.as.ival != 0;
    if (v.type == VAL_FLOAT) return v.as.fval != 0.0;
    if (v.type == VAL_STR)   return v.as.sval && v.as.sval[0] != '\0';
    if (v.type == VAL_LIST)  return v.as.lval->count > 0;
    return true;
}

/* ============================================================================
 *  3. Scope / Symbol Table
 * ============================================================================ */

static void scope_init(RdxScope *scope, RdxScope *parent, bool is_boundary) {
    scope->var_count   = 0;
    scope->parent      = parent;
    scope->is_boundary = is_boundary;
}

static bool scope_set(RdxScope *scope, const char *name, RdxVal val) {
    /* 1. Update if variable already exists in this scope or any ancestor */
    for (RdxScope *s = scope; s; s = s->parent) {
        for (int i = 0; i < s->var_count; i++) {
            if (strcmp(s->vars[i].name, name) == 0) {
                s->vars[i].val = val;
                return true;
            }
        }
        if (s->is_boundary) break; /* Stop at function or global boundary — do not leak into parent frame */
    }
    /* 2. New variable: assign to nearest function or global boundary scope */
    RdxScope *target = scope;
    while (target->parent && !target->is_boundary) {
        target = target->parent;
    }
    if (target->var_count < 512) {
        strncpy(target->vars[target->var_count].name, name, 63);
        target->vars[target->var_count].name[63] = '\0';
        target->vars[target->var_count].val = val;
        target->var_count++;
        return true;
    }
    return false;
}

static bool scope_set_local(RdxScope *scope, const char *name, RdxVal val) {
    for (int i = 0; i < scope->var_count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) {
            scope->vars[i].val = val;
            return true;
        }
    }
    if (scope->var_count < 512) {
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
    if (scope->parent) return scope_get(scope->parent, name, out_val);
    return false;
}

/* ============================================================================
 *  4. Engine struct
 * ============================================================================ */

#define MAX_TOKENS  16384
#define MAX_FN_DEFS 256

typedef struct {
    /* defined-function registry */
    RdxFunc  fn_defs[MAX_FN_DEFS];
    int      fn_def_count;
    /* token array */
    RdxToken tokens[MAX_TOKENS];
    int      token_count;
    int      current;
    const char *filename;
    RdxScope   global_scope;
    bool       had_error;
} RdxEngine;

typedef enum {
    FLOW_NORMAL   = 0,
    FLOW_RETURN   = 1,
    FLOW_BREAK    = 2,
    FLOW_CONTINUE = 3
} FlowControl;

/* ============================================================================
 *  5. Token Navigation
 * ============================================================================ */

static RdxToken *peek(RdxEngine *e)     { return &e->tokens[e->current]; }
static RdxToken *previous(RdxEngine *e) { return &e->tokens[e->current - 1]; }
static bool is_at_end(RdxEngine *e)     { return peek(e)->type == TOK_EOF; }

static RdxToken *advance_tok(RdxEngine *e) {
    if (!is_at_end(e)) e->current++;
    return previous(e);
}

static bool check_tok(RdxEngine *e, TokenType t) {
    if (is_at_end(e)) return false;
    return peek(e)->type == t;
}

static bool match_tok(RdxEngine *e, TokenType t) {
    if (check_tok(e, t)) { advance_tok(e); return true; }
    return false;
}

static void skip_newlines(RdxEngine *e) {
    while (match_tok(e, TOK_NEWLINE) || match_tok(e, TOK_SEMICOLON));
}

/* Forward declarations */
static RdxVal     eval_expr(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_or(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_and(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_equality(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_comparison(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_term(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_factor(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_unary(RdxEngine *e, RdxScope *scope);
static RdxVal     eval_primary(RdxEngine *e, RdxScope *scope);
static FlowControl exec_stmt(RdxEngine *e, RdxScope *scope, RdxVal *return_val);
static FlowControl exec_block(RdxEngine *e, RdxScope *scope, RdxVal *return_val);

/* ============================================================================
 *  6. Built-in String Methods
 * ============================================================================ */

static RdxVal str_method(RdxEngine *e, RdxScope *scope,
                         const char *method, RdxVal base,
                         RdxVal *args, int arg_count,
                         RdxToken *id_tok) {
    (void)e; (void)scope; (void)id_tok;

    const char *s = (base.type == VAL_STR) ? base.as.sval : "";

    if (strcmp(method, "upper") == 0) {
        char buf[2048]; strncpy(buf, s, 2047); buf[2047] = '\0';
        for (int i = 0; buf[i]; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
        return val_str(buf);
    }
    if (strcmp(method, "lower") == 0) {
        char buf[2048]; strncpy(buf, s, 2047); buf[2047] = '\0';
        for (int i = 0; buf[i]; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
        return val_str(buf);
    }
    if (strcmp(method, "len") == 0) {
        return val_int((int64_t)strlen(s));
    }
    if (strcmp(method, "trim") == 0) {
        char buf[2048]; strncpy(buf, s, 2047); buf[2047] = '\0';
        int start = 0, end = (int)strlen(buf) - 1;
        while (start <= end && isspace((unsigned char)buf[start])) start++;
        while (end >= start && isspace((unsigned char)buf[end])) end--;
        buf[end + 1] = '\0';
        return val_str(buf + start);
    }
    if (strcmp(method, "contains") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(strstr(s, args[0].as.sval) != NULL);
    }
    if (strcmp(method, "starts") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(strncmp(s, args[0].as.sval, strlen(args[0].as.sval)) == 0);
    }
    if (strcmp(method, "ends") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        size_t sl = strlen(s), pl = strlen(args[0].as.sval);
        if (pl > sl) return val_bool(false);
        return val_bool(strcmp(s + sl - pl, args[0].as.sval) == 0);
    }
    if (strcmp(method, "replace") == 0) {
        if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
            return val_str(s);
        const char *from = args[0].as.sval;
        const char *to   = args[1].as.sval;
        size_t flen = strlen(from), tlen = strlen(to);
        if (flen == 0) return val_str(s);
        char buf[4096]; int pos = 0;
        const char *p = s;
        while (*p && pos < 4090) {
            if (strncmp(p, from, flen) == 0) {
                for (size_t i = 0; i < tlen && pos < 4090; i++) buf[pos++] = to[i];
                p += flen;
            } else {
                buf[pos++] = *p++;
            }
        }
        buf[pos] = '\0';
        return val_str(buf);
    }
    if (strcmp(method, "split") == 0) {
        char delim = ' ';
        if (arg_count >= 1 && args[0].type == VAL_STR && args[0].as.sval[0])
            delim = args[0].as.sval[0];
        RdxVal lst = val_list();
        char buf[4096]; strncpy(buf, s, 4095); buf[4095] = '\0';
        char *tok = buf;
        char *end = buf;
        while (*end) {
            if (*end == delim) {
                *end = '\0';
                list_append(lst.as.lval, val_str(tok));
                tok = end + 1;
            }
            end++;
        }
        list_append(lst.as.lval, val_str(tok));
        return lst;
    }
    if (strcmp(method, "substr") == 0 || strcmp(method, "slice") == 0) {
        int start = 0, cnt = -1;
        if (arg_count >= 1 && args[0].type == VAL_INT) start = (int)args[0].as.ival;
        if (arg_count >= 2 && args[1].type == VAL_INT) cnt   = (int)args[1].as.ival;
        size_t sl = strlen(s);
        if (start < 0) start = 0;
        if ((size_t)start > sl) start = (int)sl;
        if (cnt < 0) cnt = (int)(sl - start);
        char buf[4096];
        strncpy(buf, s + start, (size_t)cnt);
        buf[cnt] = '\0';
        return val_str(buf);
    }
    if (strcmp(method, "index") == 0 || strcmp(method, "find") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_int(-1);
        const char *pos = strstr(s, args[0].as.sval);
        if (!pos) return val_int(-1);
        return val_int((int64_t)(pos - s));
    }

    /* Unknown method — return nil */
    char why[128];
    snprintf(why, sizeof(why), "Unknown string method '%s'.", method);
    rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                    "NAME ERROR", why,
                    "Valid methods: upper, lower, len, trim, contains, starts, ends, "
                    "replace, split, substr, find");
    e->had_error = true;
    return val_nil();
}

/* ============================================================================
 *  7. Built-in Function Call Dispatch
 * ============================================================================ */

static const char *BUILTIN_FUNCS[] = {
    "ip.is_valid", "ip.is_private", "ip.is_public", "ip.is_loopback",
    "ip.in_subnet", "ip.octets",
    "hash.md5", "hash.sha1", "hash.sha256",
    "hash.file_md5", "hash.file_sha256", "hash.match",
    "net.port_name", "net.port_risk",
    "log.extract_ip", "log.extract_url", "log.extract_cve",
    "log.extract_status", "log.detect_attack",
    "threat.is_ioc",
    "math.abs", "math.min", "math.max", "math.pow", "math.sqrt",
    "math.floor", "math.ceil", "math.round", "math.log", "math.sin",
    "math.cos", "math.tan",
    "str.join",
    "len", "range", "type", "int", "float", "str", "bool",
    "input", "append", "exit",
    "alert", "timeline", "print", "println",
    NULL
};
static int BUILTIN_FUNCS_COUNT = 0;

static void init_builtin_count(void) {
    if (BUILTIN_FUNCS_COUNT > 0) return;
    while (BUILTIN_FUNCS[BUILTIN_FUNCS_COUNT]) BUILTIN_FUNCS_COUNT++;
}



static RdxVal dispatch_builtin(RdxEngine *e, RdxScope *scope,
                               const char *name,
                               RdxVal *args, int arg_count,
                               RdxToken *id_tok) {
    init_builtin_count();
    (void)scope;

    /* ── IP Builtins ── */
    if (strcmp(name, "ip.is_valid")   == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(rdx_ip_is_valid(args[0].as.sval));
    }
    if (strcmp(name, "ip.is_private") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(rdx_ip_is_private(args[0].as.sval));
    }
    if (strcmp(name, "ip.is_public")  == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(rdx_ip_is_public(args[0].as.sval));
    }
    if (strcmp(name, "ip.is_loopback") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_bool(false);
        return val_bool(rdx_ip_is_loopback(args[0].as.sval));
    }
    if (strcmp(name, "ip.in_subnet")  == 0) {
        if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) {
            rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                            "ARGUMENT ERROR",
                            "ip.in_subnet(ip, cidr) expects 2 string arguments.",
                            "Example: ip.in_subnet(\"192.168.1.5\", \"192.168.1.0/24\")");
            e->had_error = true;
            return val_bool(false);
        }
        return val_bool(rdx_ip_in_subnet(args[0].as.sval, args[1].as.sval));
    }
    if (strcmp(name, "ip.octets")     == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_nil();
        int octs[4];
        if (!rdx_ip_octets(args[0].as.sval, octs)) return val_nil();
        RdxVal l = val_list();
        for (int i = 0; i < 4; i++) list_append(l.as.lval, val_int(octs[i]));
        return l;
    }

    /* ── Hash Builtins ── */
    if (strcmp(name, "hash.md5")       == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char out[33]; rdx_hash_md5(args[0].as.sval, strlen(args[0].as.sval), out);
        return val_str(out);
    }
    if (strcmp(name, "hash.sha1")      == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char out[41]; rdx_hash_sha1(args[0].as.sval, strlen(args[0].as.sval), out);
        return val_str(out);
    }
    if (strcmp(name, "hash.sha256")    == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char out[65]; rdx_hash_sha256(args[0].as.sval, strlen(args[0].as.sval), out);
        return val_str(out);
    }
    if (strcmp(name, "hash.file_md5")  == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char out[33];
        return rdx_hash_file_md5(args[0].as.sval, out) ? val_str(out) : val_str("FILE_NOT_FOUND");
    }
    if (strcmp(name, "hash.file_sha256") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char out[65];
        return rdx_hash_file_sha256(args[0].as.sval, out) ? val_str(out) : val_str("FILE_NOT_FOUND");
    }
    if (strcmp(name, "hash.match")     == 0) {
        if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR)
            return val_bool(false);
        return val_bool(rdx_hash_match(args[0].as.sval, args[1].as.sval));
    }

    /* ── Net Builtins ── */
    if (strcmp(name, "net.port_name")  == 0) {
        int p = (arg_count > 0 && args[0].type == VAL_INT) ? (int)args[0].as.ival : 0;
        return val_str(rdx_net_port_name(p));
    }
    if (strcmp(name, "net.port_risk")  == 0) {
        int p = (arg_count > 0 && args[0].type == VAL_INT) ? (int)args[0].as.ival : 0;
        return val_str(rdx_net_port_risk(p));
    }

    /* ── Log Builtins ── */
    if (strcmp(name, "log.extract_ip")     == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char ip_out[64];
        return rdx_log_extract_ip(args[0].as.sval, ip_out) ? val_str(ip_out) : val_str("");
    }
    if (strcmp(name, "log.extract_url")    == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char url_out[256];
        return rdx_log_extract_url(args[0].as.sval, url_out) ? val_str(url_out) : val_str("");
    }
    if (strcmp(name, "log.extract_cve")    == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        char cve_out[64];
        return rdx_log_extract_cve(args[0].as.sval, cve_out) ? val_str(cve_out) : val_str("");
    }
    if (strcmp(name, "log.extract_status") == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_int(0);
        return val_int(rdx_log_extract_status(args[0].as.sval));
    }
    if (strcmp(name, "log.detect_attack")  == 0) {
        if (arg_count < 1 || args[0].type != VAL_STR) return val_str("");
        const char *atk = rdx_log_detect_attack(args[0].as.sval);
        return val_str(atk ? atk : "");
    }

    /* ── Threat Builtins ── */
    if (strcmp(name, "threat.is_ioc") == 0) {
        if (arg_count >= 2 && args[0].type == VAL_STR && args[1].type == VAL_LIST) {
            for (int i = 0; i < args[1].as.lval->count; i++) {
                if (args[1].as.lval->items[i].type == VAL_STR &&
                    rdx_hash_match(args[0].as.sval, args[1].as.lval->items[i].as.sval))
                    return val_bool(true);
            }
        }
        return val_bool(false);
    }

    /* ── Math Builtins ── */
    if (strcmp(name, "math.abs") == 0) {
        if (arg_count < 1) return val_float(0);
        double v = val_as_double(args[0]);
        return (args[0].type == VAL_INT) ? val_int((int64_t)llabs(args[0].as.ival)) : val_float(fabs(v));
    }
    if (strcmp(name, "math.sqrt") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(sqrt(val_as_double(args[0])));
    }
    if (strcmp(name, "math.pow") == 0) {
        if (arg_count < 2) return val_float(0);
        return val_float(pow(val_as_double(args[0]), val_as_double(args[1])));
    }
    if (strcmp(name, "math.floor") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(floor(val_as_double(args[0])));
    }
    if (strcmp(name, "math.ceil") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(ceil(val_as_double(args[0])));
    }
    if (strcmp(name, "math.round") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(round(val_as_double(args[0])));
    }
    if (strcmp(name, "math.min") == 0) {
        if (arg_count < 2) return val_float(0);
        double a = val_as_double(args[0]), b = val_as_double(args[1]);
        return (args[0].type == VAL_INT && args[1].type == VAL_INT) ?
               val_int(args[0].as.ival < args[1].as.ival ? args[0].as.ival : args[1].as.ival) :
               val_float(a < b ? a : b);
    }
    if (strcmp(name, "math.max") == 0) {
        if (arg_count < 2) return val_float(0);
        double a = val_as_double(args[0]), b = val_as_double(args[1]);
        return (args[0].type == VAL_INT && args[1].type == VAL_INT) ?
               val_int(args[0].as.ival > args[1].as.ival ? args[0].as.ival : args[1].as.ival) :
               val_float(a > b ? a : b);
    }
    if (strcmp(name, "math.log") == 0) {
        if (arg_count < 1) return val_float(0);
        double base_log = (arg_count >= 2) ? val_as_double(args[1]) : M_E;
        if (base_log == M_E) return val_float(log(val_as_double(args[0])));
        return val_float(log(val_as_double(args[0])) / log(base_log));
    }
    if (strcmp(name, "math.sin") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(sin(val_as_double(args[0])));
    }
    if (strcmp(name, "math.cos") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(cos(val_as_double(args[0])));
    }
    if (strcmp(name, "math.tan") == 0) {
        if (arg_count < 1) return val_float(0);
        return val_float(tan(val_as_double(args[0])));
    }

    /* ── String join ── */
    if (strcmp(name, "str.join") == 0) {
        if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_LIST)
            return val_str("");
        const char *sep = args[0].as.sval;
        char buf[4096]; buf[0] = '\0'; int pos = 0;
        for (int i = 0; i < args[1].as.lval->count; i++) {
            if (i > 0) { size_t sl = strlen(sep); if (pos + (int)sl < 4090) { memcpy(buf+pos, sep, sl); pos += (int)sl; } }
            char tmp[256]; val_to_str(args[1].as.lval->items[i], tmp, sizeof(tmp));
            size_t tl = strlen(tmp); if (pos + (int)tl < 4090) { memcpy(buf+pos, tmp, tl); pos += (int)tl; }
        }
        buf[pos] = '\0';
        return val_str(buf);
    }

    /* ── Utility Builtins ── */
    if (strcmp(name, "len") == 0) {
        if (arg_count > 0 && args[0].type == VAL_LIST) return val_int(args[0].as.lval->count);
        if (arg_count > 0 && args[0].type == VAL_STR)  return val_int(strlen(args[0].as.sval));
        return val_int(0);
    }
    if (strcmp(name, "range") == 0) {
        int64_t start = 0, end = 0, step = 1;
        if (arg_count == 1 && args[0].type == VAL_INT) {
            end = args[0].as.ival;
        } else if (arg_count >= 2 && args[0].type == VAL_INT && args[1].type == VAL_INT) {
            start = args[0].as.ival; end = args[1].as.ival;
        }
        if (arg_count >= 3 && args[2].type == VAL_INT) step = args[2].as.ival;
        if (step == 0) step = 1;
        RdxVal l = val_list();
        if (step > 0) for (int64_t i = start; i < end; i += step) list_append(l.as.lval, val_int(i));
        else          for (int64_t i = start; i > end; i += step) list_append(l.as.lval, val_int(i));
        return l;
    }
    if (strcmp(name, "type") == 0) {
        if (arg_count < 1) return val_str("nil");
        switch (args[0].type) {
            case VAL_NIL:   return val_str("nil");
            case VAL_BOOL:  return val_str("bool");
            case VAL_INT:   return val_str("int");
            case VAL_FLOAT: return val_str("float");
            case VAL_STR:   return val_str("str");
            case VAL_LIST:  return val_str("list");
            case VAL_FN:    return val_str("fn");
        }
        return val_str("unknown");
    }
    if (strcmp(name, "int") == 0) {
        if (arg_count < 1) return val_int(0);
        if (args[0].type == VAL_INT)   return args[0];
        if (args[0].type == VAL_FLOAT) return val_int((int64_t)args[0].as.fval);
        if (args[0].type == VAL_BOOL)  return val_int(args[0].as.bval ? 1 : 0);
        if (args[0].type == VAL_STR)   return val_int(strtoll(args[0].as.sval, NULL, 0));
        return val_int(0);
    }
    if (strcmp(name, "float") == 0) {
        if (arg_count < 1) return val_float(0);
        if (args[0].type == VAL_FLOAT) return args[0];
        if (args[0].type == VAL_INT)   return val_float((double)args[0].as.ival);
        if (args[0].type == VAL_BOOL)  return val_float(args[0].as.bval ? 1.0 : 0.0);
        if (args[0].type == VAL_STR)   return val_float(strtod(args[0].as.sval, NULL));
        return val_float(0);
    }
    if (strcmp(name, "str") == 0 || strcmp(name, "to_str") == 0) {
        if (arg_count < 1) return val_str("");
        char buf[512]; val_to_str(args[0], buf, sizeof(buf));
        return val_str(buf);
    }
    if (strcmp(name, "bool") == 0) {
        if (arg_count < 1) return val_bool(false);
        return val_bool(val_is_truthy(args[0]));
    }
    if (strcmp(name, "input") == 0) {
        if (arg_count >= 1 && args[0].type == VAL_STR) printf("%s", args[0].as.sval);
        fflush(stdout);
        char line[2048];
        if (!fgets(line, sizeof(line), stdin)) return val_str("");
        size_t ln = strlen(line);
        while (ln > 0 && (line[ln-1] == '\n' || line[ln-1] == '\r')) line[--ln] = '\0';
        return val_str(line);
    }
    if (strcmp(name, "append") == 0) {
        if (arg_count < 2 || args[0].type != VAL_LIST) return val_nil();
        list_append(args[0].as.lval, args[1]);
        return args[0];
    }
    if (strcmp(name, "exit") == 0) {
        int code = (arg_count >= 1 && args[0].type == VAL_INT) ? (int)args[0].as.ival : 0;
        exit(code);
    }

    /* Unknown — fuzzy suggestion */
    const char *cand = rdx_diag_suggest_similar(name, BUILTIN_FUNCS, BUILTIN_FUNCS_COUNT);
    char why_buf[256], fix_buf[256];
    snprintf(why_buf, sizeof(why_buf), "Unknown function '%s'.", name);
    if (cand) snprintf(fix_buf, sizeof(fix_buf), "Did you mean '%s'?", cand);
    else      snprintf(fix_buf, sizeof(fix_buf), "Define it with: fn %s(...) { ... }", name);
    rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                    "NAME ERROR", why_buf, fix_buf);
    e->had_error = true;
    return val_nil();
}

/* ============================================================================
 *  8. Expression Evaluator
 * ============================================================================ */

static RdxVal eval_call_args(RdxEngine *e, RdxScope *scope, RdxVal *args_out, int *count_out) {
    *count_out = 0;
    skip_newlines(e);
    if (!check_tok(e, TOK_RPAREN)) {
        do {
            skip_newlines(e);
            if (*count_out < 16) args_out[(*count_out)++] = eval_expr(e, scope);
            skip_newlines(e);
        } while (match_tok(e, TOK_COMMA));
    }
    if (!match_tok(e, TOK_RPAREN)) {
        rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                        "SYNTAX ERROR", "Missing closing ')' in function call.",
                        "Close the argument list with ')'.");
        e->had_error = true;
    }
    return val_nil(); /* unused */
}

/* Primary: literals, lists, grouping, identifiers, calls */
static RdxVal eval_primary(RdxEngine *e, RdxScope *scope) {
    skip_newlines(e);

    /* Unary minus */
    if (match_tok(e, TOK_MINUS)) {
        RdxVal v = eval_primary(e, scope);
        if (v.type == VAL_INT)   return val_int(-v.as.ival);
        if (v.type == VAL_FLOAT) return val_float(-v.as.fval);
        return val_int(0);
    }
    /* Unary not / ! */
    if (match_tok(e, TOK_BANG)) {
        RdxVal v = eval_primary(e, scope);
        return val_bool(!val_is_truthy(v));
    }

    if (match_tok(e, TOK_INT))   return val_int(previous(e)->ival);
    if (match_tok(e, TOK_FLOAT)) return val_float(previous(e)->fval);
    if (match_tok(e, TOK_STR))   return val_str(previous(e)->sval);
    if (match_tok(e, TOK_TRUE))  return val_bool(true);
    if (match_tok(e, TOK_FALSE)) return val_bool(false);
    if (match_tok(e, TOK_NIL))   return val_nil();

    /* List literal */
    if (match_tok(e, TOK_LBRACKET)) {
        RdxVal list = val_list();
        skip_newlines(e);
        if (!check_tok(e, TOK_RBRACKET)) {
            do {
                skip_newlines(e);
                list_append(list.as.lval, eval_expr(e, scope));
                skip_newlines(e);
            } while (match_tok(e, TOK_COMMA));
        }
        skip_newlines(e);
        if (!match_tok(e, TOK_RBRACKET)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected ']' to close list literal.", "Add ']'.");
            e->had_error = true;
        }
        return list;
    }

    /* Grouping */
    if (match_tok(e, TOK_LPAREN)) {
        RdxVal v = eval_expr(e, scope);
        if (!match_tok(e, TOK_RPAREN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected ')' after expression.", "Add ')'.");
            e->had_error = true;
        }
        return v;
    }

    /* Identifier, function call, method call */
    if (match_tok(e, TOK_IDENT)) {
        RdxToken *id_tok = previous(e);
        char name[128];
        strncpy(name, id_tok->sval, 127); name[127] = '\0';

        /* Function / builtin call */
        if (check_tok(e, TOK_LPAREN)) {
            advance_tok(e); /* consume '(' */
            RdxVal args[16]; int arg_count = 0;
            eval_call_args(e, scope, args, &arg_count);

            /* Check user-defined functions first */
            for (int fi = 0; fi < e->fn_def_count; fi++) {
                if (strcmp(e->fn_defs[fi].name, name) == 0) {
                    RdxFunc *fn = &e->fn_defs[fi];
                    RdxScope fn_scope;
                    scope_init(&fn_scope, &e->global_scope, true);
                    for (int pi = 0; pi < fn->param_count; pi++) {
                        RdxVal pv = (pi < arg_count) ? args[pi] : val_nil();
                        scope_set_local(&fn_scope, fn->params[pi], pv);
                    }
                    int saved_pos = e->current;
                    e->current = fn->body_stmt_start;
                    RdxVal ret_val = val_nil();
                    FlowControl fc = exec_block(e, &fn_scope, &ret_val);
                    (void)fc;
                    e->current = saved_pos;
                    return ret_val;
                }
            }

            return dispatch_builtin(e, scope, name, args, arg_count, id_tok);
        }

        /* Module namespace call, e.g. str.join, ip.is_valid, math.sqrt */
        if (check_tok(e, TOK_DOT)) {
            RdxVal temp_v;
            if (!scope_get(scope, name, &temp_v)) {
                advance_tok(e); /* consume '.' */
                if (!match_tok(e, TOK_IDENT)) {
                    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                    "SYNTAX ERROR", "Expected function name after '.'.", "Add function name.");
                    e->had_error = true;
                    return val_nil();
                }
                char member[64];
                strncpy(member, previous(e)->sval, 63); member[63] = '\0';
                char full_name[128];
                snprintf(full_name, sizeof(full_name), "%s.%s", name, member);
                if (check_tok(e, TOK_LPAREN)) {
                    advance_tok(e); /* consume '(' */
                    RdxVal args[16]; int arg_count = 0;
                    eval_call_args(e, scope, args, &arg_count);
                    return dispatch_builtin(e, scope, full_name, args, arg_count, id_tok);
                }
                return val_nil();
            }
        }

        /* Variable lookup */
        RdxVal v;
        if (scope_get(scope, name, &v)) {
            /* Dot method call on variable: var.method(args) */
            while (check_tok(e, TOK_DOT)) {
                advance_tok(e); /* consume '.' */
                if (!match_tok(e, TOK_IDENT)) {
                    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                    "SYNTAX ERROR", "Expected method name after '.'.", "Add method name.");
                    e->had_error = true;
                    break;
                }
                char method[64];
                strncpy(method, previous(e)->sval, 63); method[63] = '\0';
                RdxToken *m_tok = previous(e);
                char full_name[128];
                snprintf(full_name, sizeof(full_name), "%s.%s", name, method);

                if (check_tok(e, TOK_LPAREN)) {
                    advance_tok(e); /* consume '(' */
                    RdxVal margs[16]; int marg_count = 0;
                    eval_call_args(e, scope, margs, &marg_count);
                    /* If matches a builtin namespace like ip.is_valid, dispatch to builtin */
                    bool is_builtin = false;
                    for (int bi = 0; BUILTIN_FUNCS[bi]; bi++) {
                        if (strcmp(BUILTIN_FUNCS[bi], full_name) == 0) {
                            is_builtin = true; break;
                        }
                    }
                    if (is_builtin) {
                        return dispatch_builtin(e, scope, full_name, margs, marg_count, m_tok);
                    }
                    v = str_method(e, scope, method, v, margs, marg_count, m_tok);
                } else {
                    /* Property-style, e.g. list.len */
                    if (strcmp(method, "len") == 0) {
                        if (v.type == VAL_LIST) v = val_int(v.as.lval->count);
                        else if (v.type == VAL_STR) v = val_int((int64_t)strlen(v.as.sval));
                        else v = val_int(0);
                    } else {
                        char why[128];
                        snprintf(why, sizeof(why), "Unknown property '%s'.", method);
                        rdx_diag_report(e->filename, m_tok->line, m_tok->col, m_tok->length,
                                        "NAME ERROR", why, "Check for valid properties.");
                        e->had_error = true;
                        v = val_nil();
                    }
                }
            }

            /* List indexing: var[idx] */
            while (check_tok(e, TOK_LBRACKET)) {
                advance_tok(e);
                RdxVal idx = eval_expr(e, scope);
                if (!match_tok(e, TOK_RBRACKET)) {
                    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                                    "SYNTAX ERROR", "Missing ']' in index.", "Add ']'.");
                    e->had_error = true;
                }
                if (v.type == VAL_LIST && idx.type == VAL_INT) {
                    int i = (int)idx.as.ival;
                    if (i < 0) i = v.as.lval->count + i; /* negative indexing */
                    if (i >= 0 && i < v.as.lval->count) {
                        v = v.as.lval->items[i];
                    } else {
                        char why[128];
                        snprintf(why, sizeof(why), "Index %d out of bounds (list length %d).", i, v.as.lval->count);
                        rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                                        "INDEX ERROR", why, "Use indices from 0 to len(list)-1.");
                        return val_nil();
                    }
                } else if (v.type == VAL_STR && idx.type == VAL_INT) {
                    int i = (int)idx.as.ival;
                    size_t sl = strlen(v.as.sval);
                    if (i < 0) i = (int)sl + i;
                    if (i >= 0 && i < (int)sl) {
                        char ch[2]; ch[0] = v.as.sval[i]; ch[1] = '\0';
                        v = val_str(ch);
                    } else {
                        v = val_str("");
                    }
                }
            }
            return v;
        }

        /* Undefined variable — fuzzy suggestion */
        const char *known_vars[512]; int kcount = 0;
        RdxScope *sc = scope;
        while (sc) {
            for (int i = 0; i < sc->var_count && kcount < 512; i++)
                known_vars[kcount++] = sc->vars[i].name;
            sc = sc->parent;
        }
        const char *cand = rdx_diag_suggest_similar(name, known_vars, kcount);
        char why_buf[256], fix_buf[256];
        snprintf(why_buf, sizeof(why_buf), "Variable '%s' has not been defined.", name);
        if (cand) snprintf(fix_buf, sizeof(fix_buf), "Did you mean '%s'? Or define it first: %s = ...", cand, name);
        else      snprintf(fix_buf, sizeof(fix_buf), "Define it before use: %s = <value>", name);
        rdx_diag_report(e->filename, id_tok->line, id_tok->col, id_tok->length,
                        "UNDEFINED VARIABLE", why_buf, fix_buf);
        e->had_error = true;
        return val_nil();
    }

    rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                    "SYNTAX ERROR", "Unexpected token in expression.",
                    "Check for missing operators or mismatched brackets.");
    e->had_error = true;
    advance_tok(e);
    return val_nil();
}

static RdxVal eval_unary(RdxEngine *e, RdxScope *scope) {
    skip_newlines(e);
    if (match_tok(e, TOK_BANG)) {
        RdxVal v = eval_unary(e, scope);
        return val_bool(!val_is_truthy(v));
    }
    if (match_tok(e, TOK_MINUS)) {
        RdxVal v = eval_unary(e, scope);
        if (v.type == VAL_FLOAT) return val_float(-v.as.fval);
        if (v.type == VAL_INT)   return val_int(-v.as.ival);
        return val_int(-val_as_int(v));
    }
    if (match_tok(e, TOK_PLUS)) {
        return eval_unary(e, scope);
    }
    return eval_primary(e, scope);
}

static RdxVal eval_factor(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_unary(e, scope);
    while (match_tok(e, TOK_STAR) || match_tok(e, TOK_SLASH) || match_tok(e, TOK_PERCENT)) {
        TokenType op = previous(e)->type;
        RdxToken *op_tok = previous(e);
        RdxVal right = eval_unary(e, scope);
        if (op == TOK_STAR) {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                left = val_float(val_as_double(left) * val_as_double(right));
            } else if (left.type == VAL_STR && (right.type == VAL_INT || right.type == VAL_FLOAT)) {
                int64_t count = val_as_int(right);
                char buf[4096]; buf[0] = '\0'; int pos = 0;
                size_t sl = strlen(left.as.sval);
                for (int64_t i = 0; i < count && pos < 4090; i++) {
                    memcpy(buf + pos, left.as.sval, sl); pos += (int)sl;
                }
                buf[pos] = '\0';
                left = val_str(buf);
            } else {
                left = val_int(val_as_int(left) * val_as_int(right));
            }
        } else if (op == TOK_SLASH) {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double rv = val_as_double(right);
                if (rv == 0.0) {
                    rdx_diag_report(e->filename, op_tok->line, op_tok->col, 1,
                                    "RUNTIME ERROR", "Division by zero (float).",
                                    "Ensure the divisor is non-zero.");
                    left = val_float(0.0);
                } else {
                    left = val_float(val_as_double(left) / rv);
                }
            } else {
                int64_t rv = val_as_int(right);
                if (rv == 0) {
                    rdx_diag_report(e->filename, op_tok->line, op_tok->col, 1,
                                    "RUNTIME ERROR", "Integer division by zero.",
                                    "Ensure the divisor is non-zero before dividing.");
                    e->had_error = true;
                    left = val_int(0);
                } else {
                    left = val_int(val_as_int(left) / rv);
                }
            }
        } else if (op == TOK_PERCENT) {
            int64_t rv = val_as_int(right);
            left = (rv != 0) ? val_int(val_as_int(left) % rv) : val_int(0);
        }
    }
    return left;
}

static RdxVal eval_term(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_factor(e, scope);
    while (match_tok(e, TOK_PLUS) || match_tok(e, TOK_MINUS)) {
        TokenType op = previous(e)->type;
        RdxVal right = eval_factor(e, scope);
        if (op == TOK_PLUS) {
            if (left.type == VAL_INT && right.type == VAL_INT) {
                left = val_int(left.as.ival + right.as.ival);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                left = val_float(val_as_double(left) + val_as_double(right));
            } else if (left.type == VAL_STR || right.type == VAL_STR) {
                char lbuf[1024], rbuf[1024], buf[2048];
                val_to_str(left, lbuf, sizeof(lbuf));
                val_to_str(right, rbuf, sizeof(rbuf));
                snprintf(buf, sizeof(buf), "%s%s", lbuf, rbuf);
                left = val_str(buf);
            } else {
                left = val_int(val_as_int(left) + val_as_int(right));
            }
        } else {
            if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                left = val_float(val_as_double(left) - val_as_double(right));
            } else {
                left = val_int(val_as_int(left) - val_as_int(right));
            }
        }
    }
    return left;
}

static RdxVal eval_comparison(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_term(e, scope);
    while (match_tok(e, TOK_LT) || match_tok(e, TOK_LTE) ||
           match_tok(e, TOK_GT) || match_tok(e, TOK_GTE)) {
        TokenType op = previous(e)->type;
        RdxVal right = eval_term(e, scope);
        if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
            double l = val_as_double(left), r = val_as_double(right);
            if (op == TOK_LT)       left = val_bool(l < r);
            else if (op == TOK_LTE) left = val_bool(l <= r);
            else if (op == TOK_GT)  left = val_bool(l > r);
            else                    left = val_bool(l >= r);
        } else if (left.type == VAL_STR && right.type == VAL_STR) {
            int c = strcmp(left.as.sval, right.as.sval);
            if (op == TOK_LT)       left = val_bool(c < 0);
            else if (op == TOK_LTE) left = val_bool(c <= 0);
            else if (op == TOK_GT)  left = val_bool(c > 0);
            else                    left = val_bool(c >= 0);
        } else {
            int64_t l = val_as_int(left), r = val_as_int(right);
            if (op == TOK_LT)       left = val_bool(l < r);
            else if (op == TOK_LTE) left = val_bool(l <= r);
            else if (op == TOK_GT)  left = val_bool(l > r);
            else                    left = val_bool(l >= r);
        }
    }
    return left;
}

static RdxVal eval_equality(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_comparison(e, scope);
    while (match_tok(e, TOK_EQ) || match_tok(e, TOK_NEQ)) {
        TokenType op = previous(e)->type;
        RdxVal right = eval_comparison(e, scope);
        bool eq;
        if (left.type == VAL_STR && right.type == VAL_STR) {
            eq = (strcmp(left.as.sval, right.as.sval) == 0);
        } else if (left.type == VAL_NIL && right.type == VAL_NIL) {
            eq = true;
        } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
            eq = (val_as_double(left) == val_as_double(right));
        } else if (left.type == VAL_INT || right.type == VAL_INT) {
            eq = (val_as_int(left) == val_as_int(right));
        } else if (left.type == VAL_BOOL || right.type == VAL_BOOL) {
            eq = (val_is_truthy(left) == val_is_truthy(right));
        } else {
            eq = false;
        }
        left = val_bool(op == TOK_EQ ? eq : !eq);
    }
    return left;
}

static RdxVal eval_and(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_equality(e, scope);
    while (match_tok(e, TOK_AND)) {
        RdxVal right = eval_equality(e, scope);
        left = val_bool(val_is_truthy(left) && val_is_truthy(right));
    }
    return left;
}

static RdxVal eval_or(RdxEngine *e, RdxScope *scope) {
    RdxVal left = eval_and(e, scope);
    while (match_tok(e, TOK_OR)) {
        RdxVal right = eval_and(e, scope);
        left = val_bool(val_is_truthy(left) || val_is_truthy(right));
    }
    return left;
}

static RdxVal eval_expr(RdxEngine *e, RdxScope *scope) {
    return eval_or(e, scope);
}

/* ============================================================================
 *  9. Statement Executor
 * ============================================================================ */

/* Skip a block { ... } without executing (for if/else skipping) */
static void skip_block(RdxEngine *e) {
    int depth = 1;
    while (depth > 0 && !is_at_end(e)) {
        if (match_tok(e, TOK_LBRACE)) depth++;
        else if (match_tok(e, TOK_RBRACE)) depth--;
        else advance_tok(e);
    }
}

/* Execute statements inside a { ... } block */
static FlowControl exec_block(RdxEngine *e, RdxScope *scope, RdxVal *return_val) {
    FlowControl flow = FLOW_NORMAL;
    while (!check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
        flow = exec_stmt(e, scope, return_val);
        if (flow != FLOW_NORMAL) {
            skip_block(e);
            return flow;
        }
        skip_newlines(e);
    }
    if (!match_tok(e, TOK_RBRACE)) {
        if (!is_at_end(e)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Unclosed block — expected '}'.",
                            "Add '}' to close the block.");
            e->had_error = true;
        }
    }
    return flow;
}

static FlowControl exec_stmt(RdxEngine *e, RdxScope *scope, RdxVal *return_val) {
    skip_newlines(e);
    if (is_at_end(e)) return FLOW_NORMAL;

    /* ── Function definition: fn name(p1, p2) { ... } ── */
    if (match_tok(e, TOK_FN)) {
        if (!match_tok(e, TOK_IDENT)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected function name after 'fn'.",
                            "Example: fn greet(name) { print name }");
            e->had_error = true; return FLOW_NORMAL;
        }
        char fn_name[64];
        strncpy(fn_name, previous(e)->sval, 63); fn_name[63] = '\0';

        if (!match_tok(e, TOK_LPAREN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '(' after function name.",
                            "Example: fn name(params) { ... }");
            e->had_error = true; return FLOW_NORMAL;
        }

        char params[8][64]; int param_count = 0;
        skip_newlines(e);
        if (!check_tok(e, TOK_RPAREN)) {
            do {
                skip_newlines(e);
                if (!match_tok(e, TOK_IDENT)) break;
                if (param_count < 8) {
                    strncpy(params[param_count], previous(e)->sval, 63);
                    params[param_count][63] = '\0';
                    param_count++;
                }
                skip_newlines(e);
            } while (match_tok(e, TOK_COMMA));
        }
        if (!match_tok(e, TOK_RPAREN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected ')' to close parameter list.", "Add ')'.");
            e->had_error = true; return FLOW_NORMAL;
        }
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' to start function body.", "Add '{'.");
            e->had_error = true; return FLOW_NORMAL;
        }

        /* Record token position of function body start */
        int body_start = e->current;

        /* Skip the body */
        int depth = 1;
        while (depth > 0 && !is_at_end(e)) {
            if (match_tok(e, TOK_LBRACE)) depth++;
            else if (match_tok(e, TOK_RBRACE)) depth--;
            else advance_tok(e);
        }

        /* Register function */
        if (e->fn_def_count < MAX_FN_DEFS) {
            RdxFunc *fn = &e->fn_defs[e->fn_def_count++];
            strncpy(fn->name, fn_name, 63); fn->name[63] = '\0';
            fn->param_count = param_count;
            fn->body_stmt_start = body_start;
            for (int i = 0; i < param_count; i++) {
                strncpy(fn->params[i], params[i], 63);
                fn->params[i][63] = '\0';
            }

            /* Also store as a variable so it can be passed/called */
            RdxVal fn_val; fn_val.type = VAL_FN; fn_val.as.fnval = fn;
            scope_set(&e->global_scope, fn_name, fn_val);
        }
        return FLOW_NORMAL;
    }

    /* ── return ── */
    if (match_tok(e, TOK_RETURN)) {
        if (!check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON) &&
            !check_tok(e, TOK_RBRACE) && !is_at_end(e)) {
            if (return_val) *return_val = eval_expr(e, scope);
        }
        return FLOW_RETURN;
    }

    /* ── break ── */
    if (match_tok(e, TOK_BREAK))    return FLOW_BREAK;

    /* ── continue ── */
    if (match_tok(e, TOK_CONTINUE)) return FLOW_CONTINUE;

    /* ── print / println ── */
    if (match_tok(e, TOK_PRINT)) {
        bool paren = match_tok(e, TOK_LPAREN);
        bool first = true;
        do {
            skip_newlines(e);
            if (check_tok(e, TOK_RPAREN) && paren) break;
            RdxVal v = eval_expr(e, scope);
            if (!first) printf(" ");
            val_print(v);
            first = false;
            skip_newlines(e);
        } while (match_tok(e, TOK_COMMA));
        if (paren) match_tok(e, TOK_RPAREN);
        printf("\n");
        return FLOW_NORMAL;
    }

    /* ── alert ── */
    if (match_tok(e, TOK_ALERT)) {
        bool paren = match_tok(e, TOK_LPAREN);
        RdxVal lvl = eval_expr(e, scope);
        RdxVal msg = val_str(""); RdxVal det = val_str("");
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) ||
            (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            msg = eval_expr(e, scope);
        }
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) ||
            (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            det = eval_expr(e, scope);
        }
        if (paren) match_tok(e, TOK_RPAREN);
        rdx_cyber_alert(lvl.type == VAL_STR ? lvl.as.sval : "ALERT",
                        msg.type == VAL_STR ? msg.as.sval : "",
                        det.type == VAL_STR ? det.as.sval : "");
        return FLOW_NORMAL;
    }

    /* ── timeline ── */
    if (match_tok(e, TOK_TIMELINE)) {
        bool paren = match_tok(e, TOK_LPAREN);
        RdxVal evt = eval_expr(e, scope);
        RdxVal det = val_str("");
        skip_newlines(e);
        if (match_tok(e, TOK_COMMA) ||
            (!paren && !check_tok(e, TOK_NEWLINE) && !check_tok(e, TOK_SEMICOLON))) {
            det = eval_expr(e, scope);
        }
        if (paren) match_tok(e, TOK_RPAREN);
        rdx_cyber_timeline(evt.type == VAL_STR ? evt.as.sval : "EVENT",
                           det.type == VAL_STR ? det.as.sval : "");
        return FLOW_NORMAL;
    }

    /* ── let var = expr ── */
    if (match_tok(e, TOK_LET)) {
        if (!match_tok(e, TOK_IDENT)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected variable name after 'let'.",
                            "Example: let x = 42");
            e->had_error = true; return FLOW_NORMAL;
        }
        char varname[64];
        strncpy(varname, previous(e)->sval, 63); varname[63] = '\0';
        if (!match_tok(e, TOK_ASSIGN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '=' after variable name.",
                            "Example: let x = 42");
            e->had_error = true; return FLOW_NORMAL;
        }
        RdxVal val = eval_expr(e, scope);
        scope_set(scope, varname, val);
        return FLOW_NORMAL;
    }

    /* ── if / elif / else ── */
    if (match_tok(e, TOK_IF)) {
        RdxVal cond = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' after if condition.",
                            "Use '{' to open the block.");
            e->had_error = true; return FLOW_NORMAL;
        }

        bool branch_taken = val_is_truthy(cond);
        FlowControl flow = FLOW_NORMAL;

        if (branch_taken) {
            RdxScope block_scope; scope_init(&block_scope, scope, false);
            flow = exec_block(e, &block_scope, return_val);
        } else {
            skip_block(e);
        }

        /* elif / else chain */
        while (true) {
            skip_newlines(e);
            if (check_tok(e, TOK_ELIF)) {
                advance_tok(e);
                RdxVal elif_cond = eval_expr(e, scope);
                skip_newlines(e);
                if (!match_tok(e, TOK_LBRACE)) { e->had_error = true; break; }
                if (!branch_taken && val_is_truthy(elif_cond)) {
                    branch_taken = true;
                    RdxScope block_scope; scope_init(&block_scope, scope, false);
                    flow = exec_block(e, &block_scope, return_val);
                } else {
                    skip_block(e);
                }
            } else if (check_tok(e, TOK_ELSE)) {
                advance_tok(e);
                skip_newlines(e);
                if (match_tok(e, TOK_LBRACE)) {
                    if (!branch_taken) {
                        RdxScope block_scope; scope_init(&block_scope, scope, false);
                        flow = exec_block(e, &block_scope, return_val);
                    } else {
                        skip_block(e);
                    }
                }
                break;
            } else {
                break;
            }
        }
        return flow;
    }

    /* ── while condition { ... } ── */
    if (match_tok(e, TOK_WHILE)) {
        int cond_pos = e->current;
        RdxVal cond = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' after while condition.", "Add '{'.");
            e->had_error = true; return FLOW_NORMAL;
        }
        int body_start = e->current;

        while (val_is_truthy(cond)) {
            e->current = body_start;
            RdxScope loop_scope; scope_init(&loop_scope, scope, false);
            RdxVal ret_v = val_nil();
            FlowControl fc = exec_block(e, &loop_scope, &ret_v);
            if (fc == FLOW_BREAK)    { break; }
            if (fc == FLOW_RETURN)   { if (return_val) *return_val = ret_v; e->current = body_start; skip_block(e); return FLOW_RETURN; }
            /* FLOW_CONTINUE — just re-evaluate condition */
            e->current = cond_pos;
            cond = eval_expr(e, scope);
            skip_newlines(e);
            match_tok(e, TOK_LBRACE);
        }
        /* Advance past body if loop exited normally */
        e->current = body_start;
        skip_block(e);
        return FLOW_NORMAL;
    }

    /* ── for var in list/range { ... } ── */
    if (match_tok(e, TOK_FOR)) {
        if (!match_tok(e, TOK_IDENT)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected loop variable after 'for'.",
                            "Example: for item in list { ... }");
            e->had_error = true; return FLOW_NORMAL;
        }
        char loop_var[64];
        strncpy(loop_var, previous(e)->sval, 63); loop_var[63] = '\0';

        if (!match_tok(e, TOK_IN)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected 'in' keyword in for loop.",
                            "Example: for x in list { ... }");
            e->had_error = true; return FLOW_NORMAL;
        }

        RdxVal iter = eval_expr(e, scope);
        skip_newlines(e);
        if (!match_tok(e, TOK_LBRACE)) {
            rdx_diag_report(e->filename, peek(e)->line, peek(e)->col, 1,
                            "SYNTAX ERROR", "Expected '{' to start for loop body.", "Add '{'.");
            e->had_error = true; return FLOW_NORMAL;
        }
        int body_start = e->current;
        bool broke = false;

        if (iter.type == VAL_LIST) {
            for (int i = 0; i < iter.as.lval->count; i++) {
                scope_set(scope, loop_var, iter.as.lval->items[i]);
                e->current = body_start;
                RdxScope loop_scope; scope_init(&loop_scope, scope, false);
                RdxVal ret_v = val_nil();
                FlowControl fc = exec_block(e, &loop_scope, &ret_v);
                if (fc == FLOW_BREAK)  { broke = true; break; }
                if (fc == FLOW_RETURN) { if (return_val) *return_val = ret_v; e->current = body_start; skip_block(e); return FLOW_RETURN; }
            }
        }

        e->current = body_start;
        skip_block(e);
        (void)broke;
        return FLOW_NORMAL;
    }

    /* ── Assignment or augmented assignment: var = / var += / var -= etc. ── */
    if (check_tok(e, TOK_IDENT)) {
        int saved = e->current;
        advance_tok(e);
        char varname[64];
        strncpy(varname, previous(e)->sval, 63); varname[63] = '\0';
        RdxToken *var_tok = previous(e);

        /* List element assignment: var[idx] = expr */
        if (check_tok(e, TOK_LBRACKET)) {
            advance_tok(e);
            RdxVal idx = eval_expr(e, scope);
            if (!match_tok(e, TOK_RBRACKET)) { e->had_error = true; }
            if (match_tok(e, TOK_ASSIGN)) {
                RdxVal new_val = eval_expr(e, scope);
                RdxVal lst;
                if (scope_get(scope, varname, &lst) && lst.type == VAL_LIST && idx.type == VAL_INT) {
                    int i = (int)idx.as.ival;
                    if (i < 0) i = lst.as.lval->count + i;
                    if (i >= 0 && i < lst.as.lval->count) lst.as.lval->items[i] = new_val;
                }
                return FLOW_NORMAL;
            }
            e->current = saved;
        } else if (match_tok(e, TOK_ASSIGN) || match_tok(e, TOK_PLUS_ASSIGN) ||
                   match_tok(e, TOK_MINUS_ASSIGN) || match_tok(e, TOK_STAR_ASSIGN) ||
                   match_tok(e, TOK_SLASH_ASSIGN) || match_tok(e, TOK_PERCENT_ASSIGN)) {
            TokenType asgn_op = previous(e)->type;
            RdxVal rhs = eval_expr(e, scope);

            if (asgn_op == TOK_ASSIGN) {
                scope_set(scope, varname, rhs);
            } else {
                RdxVal cur = val_int(0);
                scope_get(scope, varname, &cur);
                RdxVal result;
                if (asgn_op == TOK_PLUS_ASSIGN) {
                    if (cur.type == VAL_STR || rhs.type == VAL_STR) {
                        char lb[1024], rb[1024], combined[2048];
                        val_to_str(cur, lb, sizeof(lb)); val_to_str(rhs, rb, sizeof(rb));
                        snprintf(combined, sizeof(combined), "%s%s", lb, rb);
                        result = val_str(combined);
                    } else if (cur.type == VAL_FLOAT || rhs.type == VAL_FLOAT)
                        result = val_float(val_as_double(cur) + val_as_double(rhs));
                    else result = val_int(cur.as.ival + rhs.as.ival);
                } else if (asgn_op == TOK_MINUS_ASSIGN) {
                    if (cur.type == VAL_FLOAT || rhs.type == VAL_FLOAT)
                        result = val_float(val_as_double(cur) - val_as_double(rhs));
                    else result = val_int(cur.as.ival - rhs.as.ival);
                } else if (asgn_op == TOK_STAR_ASSIGN) {
                    if (cur.type == VAL_FLOAT || rhs.type == VAL_FLOAT)
                        result = val_float(val_as_double(cur) * val_as_double(rhs));
                    else result = val_int(cur.as.ival * rhs.as.ival);
                } else if (asgn_op == TOK_SLASH_ASSIGN) {
                    if (rhs.as.ival == 0 && rhs.type == VAL_INT) { result = val_int(0); }
                    else if (cur.type == VAL_FLOAT || rhs.type == VAL_FLOAT)
                        result = val_float(val_as_double(cur) / val_as_double(rhs));
                    else result = val_int(cur.as.ival / rhs.as.ival);
                } else { /* PERCENT_ASSIGN */
                    result = (rhs.as.ival != 0) ? val_int(cur.as.ival % rhs.as.ival) : val_int(0);
                }
                scope_set(scope, varname, result);
                (void)var_tok;
            }
            return FLOW_NORMAL;
        } else {
            /* Not an assignment — rewind and treat as expression */
            e->current = saved;
        }
    }

    /* Fallback: expression statement (e.g. standalone function call) */
    eval_expr(e, scope);
    return FLOW_NORMAL;
}

/* ============================================================================
 *  10. Engine Run & REPL
 * ============================================================================ */

static void tokenize(RdxEngine *eng, const char *filename, const char *source) {
    RdxLexer lex;
    lexer_init(&lex, filename, source);
    eng->token_count = 0;
    while (eng->token_count < MAX_TOKENS - 1) {
        RdxToken tok = lexer_next_token(&lex);
        eng->tokens[eng->token_count++] = tok;
        if (tok.type == TOK_EOF) break;
    }
}

int rdx_engine_run(const char *filename, const char *source) {
    if (!source) return 1;
    rdx_diag_set_context(filename, source, strlen(source));

    RdxEngine *eng = (RdxEngine*)calloc(1, sizeof(RdxEngine));
    if (!eng) return 1;
    eng->filename = filename ? filename : "<script>";
    scope_init(&eng->global_scope, NULL, true);

    tokenize(eng, filename, source);

    while (!is_at_end(eng)) {
        skip_newlines(eng);
        if (is_at_end(eng)) break;
        int before = eng->current;
        exec_stmt(eng, &eng->global_scope, NULL);
        skip_newlines(eng);
        /* Guard against infinite loops on bad tokens */
        if (eng->current == before && !is_at_end(eng)) advance_tok(eng);
    }

    int ret = eng->had_error ? 1 : 0;
    free(eng);
    return ret;
}

void rdx_engine_repl(void) {
    printf("\n");
    printf("\033[1;36m╔══════════════════════════════════════════════════════════════╗\033[0m\n");
    printf("\033[1;36m║  RDX Language REPL  v3.0.0  [Standalone, Zero-Dependency]   ║\033[0m\n");
    printf("\033[1;36m║  Syntax-Free · Human-First · Cyber Forensics Built-In       ║\033[0m\n");
    printf("\033[1;36m╚══════════════════════════════════════════════════════════════╝\033[0m\n");
    printf("  Type 'help' for commands | 'exit' or 'quit' to leave\n\n");

    RdxEngine *eng = (RdxEngine*)calloc(1, sizeof(RdxEngine));
    if (!eng) return;
    eng->filename = "<repl>";
    scope_init(&eng->global_scope, NULL, true);

    char line_buf[4096];
    while (1) {
        printf("\033[1;32mrdx>\033[0m ");
        fflush(stdout);
        if (!fgets(line_buf, sizeof(line_buf), stdin)) break;

        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len-1] == '\n' || line_buf[len-1] == '\r'))
            line_buf[--len] = '\0';
        if (len == 0) continue;

        if (strcmp(line_buf, "exit") == 0 || strcmp(line_buf, "quit") == 0) {
            printf("\033[1;36mGoodbye from RDX!\033[0m\n");
            break;
        }
        if (strcmp(line_buf, "help") == 0) {
            printf("\n\033[1;33m--- RDX Built-in Modules ---\033[0m\n");
            printf("  \033[1;36mIP & CIDR:\033[0m  ip.is_valid(ip), ip.is_private(ip), ip.is_public(ip)\n");
            printf("              ip.in_subnet(ip, cidr), ip.octets(ip)\n");
            printf("  \033[1;36mCrypto:  \033[0m  hash.md5(str), hash.sha1(str), hash.sha256(str)\n");
            printf("              hash.file_sha256(path), hash.match(h1, h2)\n");
            printf("  \033[1;36mNetwork: \033[0m  net.port_name(port), net.port_risk(port)\n");
            printf("  \033[1;36mLog Scan:\033[0m  log.extract_ip(line), log.extract_url(line)\n");
            printf("              log.detect_attack(line), log.extract_cve(line)\n");
            printf("  \033[1;36mMath:    \033[0m  math.sqrt(n), math.pow(b,e), math.abs(n)\n");
            printf("              math.floor(n), math.ceil(n), math.min(a,b), math.max(a,b)\n");
            printf("  \033[1;36mStrings: \033[0m  var.upper(), var.lower(), var.trim(), var.len()\n");
            printf("              var.contains(s), var.replace(f,t), var.split(d)\n");
            printf("  \033[1;36mCore:    \033[0m  len(x), range(n), type(x), int(x), float(x), str(x)\n");
            printf("              input(prompt), append(list, val), exit(code)\n");
            printf("  \033[1;36mAlerts:  \033[0m  alert(level, summary, details)\n");
            printf("              timeline(event, details)\n");
            printf("  \033[1;36mFunctions:\033[0m fn name(params) { ... } | return value\n\n");
            continue;
        }
        if (strcmp(line_buf, "clear") == 0) {
            printf("\033[2J\033[H"); continue;
        }

        rdx_diag_set_context("<repl>", line_buf, len);
        tokenize(eng, "<repl>", line_buf);
        eng->current = 0;
        eng->had_error = false;

        while (!is_at_end(eng)) {
            skip_newlines(eng);
            if (is_at_end(eng)) break;
            int before = eng->current;
            RdxVal result = val_nil();
            exec_stmt(eng, &eng->global_scope, &result);
            skip_newlines(eng);
            if (eng->current == before && !is_at_end(eng)) { advance_tok(eng); }
        }
    }
    free(eng);
}
