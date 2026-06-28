/*
 * lexer.c — RDX Tokeniser: Full Implementation
 * Target : x86-64 Windows, MinGW -nostdlib -lkernel32
 * Deps   : core.h, lexer.h
 *
 * Compile the compiler (after all phases):
 *   gcc -nostdlib -O2 -o rdxc.exe src/main.c -lkernel32
 *
 * Standalone lexer smoke-test:
 *   gcc -nostdlib -O2 -o test_lexer.exe src/test_lexer.c -lkernel32
 */

#include "lexer.h"

/* ============================================================
 *  §1  Character-class helpers
 * ============================================================ */

static inline bool lx_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
            c == '_';
}
static inline bool lx_is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool lx_is_alnum(char c) { return lx_is_alpha(c) || lx_is_digit(c); }

/* ============================================================
 *  §2  Cursor helpers  (never access L->source[] directly outside these)
 * ============================================================ */

/* Look at current char without consuming. */
static inline char lx_peek(const Lexer *L) {
    if (L->pos >= L->length) return '\0';
    return L->source[L->pos];
}

/* Look one character ahead without consuming. */
static inline char lx_peek2(const Lexer *L) {
    if (L->pos + 1 >= L->length) return '\0';
    return L->source[L->pos + 1];
}

/* Consume and return the current character. Updates line counter. */
static inline char lx_advance(Lexer *L) {
    char c = L->source[L->pos++];
    if (c == '\n') L->line++;
    return c;
}

/* ============================================================
 *  §3  Skip whitespace and // line comments
 * ============================================================ */

static void lx_skip(Lexer *L) {
    while (L->pos < L->length) {
        char c = lx_peek(L);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lx_advance(L);
        } else if (c == '/' && lx_peek2(L) == '/') {
            /* Consume to end of line */
            while (L->pos < L->length && lx_peek(L) != '\n')
                L->pos++;
        } else {
            break;
        }
    }
}

/* ============================================================
 *  §4  Keyword lookup table
 * ============================================================ */

typedef struct { const char *word; TokenType type; } KWEntry;

static const KWEntry kw_table[] = {
    { "num",       TOK_NUM       },
    { "str",       TOK_STR       },
    { "fn",        TOK_FN        },
    { "return",    TOK_RETURN    },
    { "if",        TOK_IF        },
    { "else",      TOK_ELSE      },
    { "loop",      TOK_LOOP      },
    { "blueprint", TOK_BLUEPRINT },
    { "alloc",     TOK_ALLOC     },
    { "drop",      TOK_DROP      },
    { "sys",       TOK_SYS       },
    { "call",      TOK_CALL      },
    { NULL, 0 }
};

static TokenType lx_lookup_kw(const char *word) {
    for (int i = 0; kw_table[i].word != NULL; i++)
        if (my_strcmp(word, kw_table[i].word) == 0)
            return kw_table[i].type;
    return TOK_IDENT;
}

/* ============================================================
 *  §5  Error reporter
 * ============================================================ */

static void lx_error(Lexer *L, int line, const char *msg) {
    L->had_error = 1;
    rdx_print_err("[RDX LEX ERROR] line ");
    /* Convert line to string manually (no printf) */
    char lbuf[12];
    char *p = lbuf + 11;
    *p = '\0';
    int tmp = line;
    if (tmp <= 0) { *--p = '0'; }
    else { while (tmp) { *--p = '0' + (tmp % 10); tmp /= 10; } }
    rdx_print_err(p);
    rdx_print_err(": ");
    rdx_print_err(msg);
    rdx_print_err("\r\n");
}

/* ============================================================
 *  §6  Token emitter
 * ============================================================ */

static void lx_emit(Lexer *L, TokenType type, const char *val, int line) {
    if (L->token_count >= (int)MAX_TOKENS) {
        lx_error(L, line, "token buffer overflow");
        return;
    }
    Token *t = &L->tokens[L->token_count++];
    t->type   = type;
    t->line   = line;
    my_strncpy(t->value, val, 255);
    t->value[255] = '\0';
}

/* ============================================================
 *  §7  Scanning routines
 * ============================================================ */

/* --- Identifier or keyword --- */
static void lx_scan_ident(Lexer *L) {
    int   start_line = L->line;
    usize start      = L->pos;
    while (L->pos < L->length && lx_is_alnum(lx_peek(L)))
        L->pos++;
    usize len = L->pos - start;
    if (len > 255) len = 255;

    char word[256];
    my_memcpy(word, L->source + start, len);
    word[len] = '\0';

    lx_emit(L, lx_lookup_kw(word), word, start_line);
}

/* --- Integer literal (decimal only in V1) --- */
static void lx_scan_number(Lexer *L) {
    int   start_line = L->line;
    usize start      = L->pos;
    while (L->pos < L->length && lx_is_digit(lx_peek(L)))
        L->pos++;
    usize len = L->pos - start;
    if (len > 63) len = 63;

    char buf[64];
    my_memcpy(buf, L->source + start, len);
    buf[len] = '\0';
    lx_emit(L, TOK_NUM_LIT, buf, start_line);
}

/* --- String literal  "..."  with escape sequences ---
 *
 *  Supported escapes:  \" \\ \n \r \t \0
 *  The stored value is the decoded content (not the raw source).
 *  The enclosing '"' characters are NOT included in the value.
 */
static void lx_scan_string(Lexer *L) {
    int start_line = L->line;
    L->pos++;   /* consume opening '"' */

    /* Decoded bytes go into a static local buffer limited to 4 KiB */
    char buf[4097];
    usize out = 0;

    while (L->pos < L->length) {
        char c = L->source[L->pos];

        if (c == '"') { L->pos++; break; }    /* closing quote */

        if (c == '\n' || c == '\r') {
            lx_error(L, start_line, "unterminated string literal");
            break;
        }

        if (c == '\\') {
            L->pos++;
            if (L->pos >= L->length) { lx_error(L, start_line, "truncated escape"); break; }
            char esc = L->source[L->pos++];
            switch (esc) {
                case 'n':  buf[out++] = '\n'; break;
                case 'r':  buf[out++] = '\r'; break;
                case 't':  buf[out++] = '\t'; break;
                case '0':  buf[out++] = '\0'; break;
                case '\\': buf[out++] = '\\'; break;
                case '"':  buf[out++] = '"';  break;
                default:
                    buf[out++] = '\\';
                    buf[out++] = esc;
                    break;
            }
        } else {
            buf[out++] = c;
            L->pos++;
        }

        if (out >= 4096) {
            lx_error(L, start_line, "string literal too long");
            break;
        }
    }

    buf[out] = '\0';
    lx_emit(L, TOK_STR_LIT, buf, start_line);
}

/* ============================================================
 *  §8  Main dispatch — produce the next token from current position
 * ============================================================ */

static void lx_next(Lexer *L) {
    lx_skip(L);

    if (L->pos >= L->length) {
        lx_emit(L, TOK_EOF, "", L->line);
        return;
    }

    int  cur_line = L->line;
    char c        = lx_peek(L);

    /* Identifier / keyword */
    if (lx_is_alpha(c))  { lx_scan_ident(L);  return; }
    /* Numeric literal    */
    if (lx_is_digit(c))  { lx_scan_number(L); return; }
    /* String literal     */
    if (c == '"')        { lx_scan_string(L); return; }

    /* Consume `c` for all operator / punctuation cases below */
    lx_advance(L);

    switch (c) {
        /* Simple single-character tokens */
        case '+': lx_emit(L, TOK_PLUS,      "+",  cur_line); break;
        case '*': lx_emit(L, TOK_STAR,      "*",  cur_line); break;
        case '(': lx_emit(L, TOK_LPAREN,    "(",  cur_line); break;
        case ')': lx_emit(L, TOK_RPAREN,    ")",  cur_line); break;
        case '{': lx_emit(L, TOK_LBRACE,    "{",  cur_line); break;
        case '}': lx_emit(L, TOK_RBRACE,    "}",  cur_line); break;
        case ';': lx_emit(L, TOK_SEMICOLON, ";",  cur_line); break;
        case ',': lx_emit(L, TOK_COMMA,     ",",  cur_line); break;
        case '.': lx_emit(L, TOK_DOT,       ".",  cur_line); break;
        case ':': lx_emit(L, TOK_ASSIGN,    ":",  cur_line); break;

        /* Slash — division operator (// already consumed by lx_skip) */
        case '/': lx_emit(L, TOK_SLASH, "/", cur_line); break;

        /* Minus or Arrow  ->  */
        case '-':
            if (lx_peek(L) == '>') {
                lx_advance(L);
                lx_emit(L, TOK_ARROW, "->", cur_line);
            } else {
                lx_emit(L, TOK_MINUS, "-", cur_line);
            }
            break;

        /* == only (bare = is not valid RDX syntax) */
        case '=':
            if (lx_peek(L) == '=') {
                lx_advance(L);
                lx_emit(L, TOK_EQ, "==", cur_line);
            } else {
                lx_error(L, cur_line, "unexpected '=' — did you mean '=='?");
                lx_emit(L, TOK_UNKNOWN, "=", cur_line);
            }
            break;

        /* != */
        case '!':
            if (lx_peek(L) == '=') {
                lx_advance(L);
                lx_emit(L, TOK_NEQ, "!=", cur_line);
            } else {
                lx_error(L, cur_line, "unexpected '!' — did you mean '!='?");
                lx_emit(L, TOK_UNKNOWN, "!", cur_line);
            }
            break;

        /* < or <= */
        case '<':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_LTE, "<=", cur_line); }
            else                   {                 lx_emit(L, TOK_LT,  "<",  cur_line); }
            break;

        /* > or >= */
        case '>':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_GTE, ">=", cur_line); }
            else                   {                 lx_emit(L, TOK_GT,  ">",  cur_line); }
            break;

        /* Anything else is an error */
        default: {
            char msg[48] = "unexpected character: '?'";
            msg[23] = c;
            lx_error(L, cur_line, msg);
            char unk[2] = { c, '\0' };
            lx_emit(L, TOK_UNKNOWN, unk, cur_line);
            break;
        }
    }
}

/* ============================================================
 *  §9  Public API
 * ============================================================ */

void lexer_init(Lexer *L, const char *source) {
    L->source      = source;
    L->pos         = 0;
    L->length      = my_strlen(source);
    L->line        = 1;
    L->token_count = 0;
    L->had_error   = 0;
    L->tokens      = (Token*)rdx_alloc_bump(sizeof(Token) * MAX_TOKENS);
    if (L->tokens == NULL)
        rdx_panic("lexer_init: OOM — token array allocation failed");
}

int lexer_tokenize(Lexer *L) {
    while (1) {
        lx_next(L);
        if (L->token_count > 0 &&
            L->tokens[L->token_count - 1].type == TOK_EOF)
            break;
        if (L->token_count >= (int)MAX_TOKENS) break;
    }
    return L->had_error ? -1 : L->token_count;
}

const char *token_type_name(TokenType t) {
    static const char *names[TOK_COUNT + 1] = {
        "TOK_NUM_LIT",  "TOK_STR_LIT",
        "TOK_NUM",      "TOK_STR",
        "TOK_FN",       "TOK_RETURN",    "TOK_IF",        "TOK_ELSE",
        "TOK_LOOP",     "TOK_BLUEPRINT", "TOK_ALLOC",     "TOK_DROP",
        "TOK_SYS",      "TOK_CALL",
        "TOK_IDENT",
        "TOK_PLUS",     "TOK_MINUS",     "TOK_STAR",      "TOK_SLASH",
        "TOK_EQ",       "TOK_NEQ",       "TOK_LT",        "TOK_GT",
        "TOK_LTE",      "TOK_GTE",
        "TOK_ASSIGN",   "TOK_ARROW",
        "TOK_LPAREN",   "TOK_RPAREN",
        "TOK_LBRACE",   "TOK_RBRACE",
        "TOK_SEMICOLON","TOK_COMMA",     "TOK_DOT",
        "TOK_EOF",      "TOK_UNKNOWN",
        NULL
    };
    if ((int)t < 0 || t >= TOK_COUNT) return "TOK_???";
    return names[t];
}

void lexer_dump(const Lexer *L) {
    rdx_print("=== Lexer Token Dump ===\r\n");
    for (int i = 0; i < L->token_count; i++) {
        const Token *t = &L->tokens[i];
        /* Pad index to 4 digits */
        rdx_print("  [");
        rdx_print_num((i64)i);
        rdx_print("] L");
        rdx_print_num((i64)t->line);
        rdx_print("  ");
        /* Pad type name to 20 chars */
        const char *name = token_type_name(t->type);
        rdx_print(name);
        /* Print spaces to align value column */
        usize nlen = my_strlen(name);
        while (nlen++ < 20) rdx_print_char(' ');
        rdx_print("  \"");
        rdx_print(t->value);
        rdx_print("\"\r\n");
    }
    rdx_print("=== ");
    rdx_print_num((i64)L->token_count);
    rdx_print(" tokens");
    if (L->had_error) rdx_print("  [HAD ERRORS]");
    rdx_print(" ===\r\n");
}
