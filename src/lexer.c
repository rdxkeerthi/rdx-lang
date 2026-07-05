/*
 * lexer.c — RDX Tokeniser (v2 — simplified syntax)
 * Target : x86-64 Windows, MinGW -nostdlib -lkernel32
 */
#include "lexer.h"
#include "diag.h"/* ============================================================ §1 Char helpers */
static inline bool lx_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline bool lx_is_digit(char c) { return c >= '0' && c <= '9'; }
static inline bool lx_is_alnum(char c) { return lx_is_alpha(c) || lx_is_digit(c); }

/* ============================================================ §2 Cursor */
static inline char lx_peek (const Lexer *L) {
    return (L->pos < L->length) ? L->source[L->pos] : '\0';
}
static inline char lx_peek2(const Lexer *L) {
    return (L->pos + 1 < L->length) ? L->source[L->pos + 1] : '\0';
}
static inline char lx_advance(Lexer *L) {
    char c = L->source[L->pos++];
    if (c == '\n') { L->line++; L->line_start_pos = L->pos; }
    return c;
}

/* ============================================================ §3 Skip whitespace + // comments */
static void lx_skip(Lexer *L) {
    while (L->pos < L->length) {
        char c = lx_peek(L);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            lx_advance(L);
        } else if (c == '/' && lx_peek2(L) == '/') {
            while (L->pos < L->length && lx_peek(L) != '\n') L->pos++;
        } else {
            break;
        }
    }
}

/* ============================================================ §4 Keyword table */
typedef struct { const char *word; TokenType type; } KWEntry;

static const KWEntry kw_table[] = {
    /* 3-letter type keywords */
    { "num",   TOK_NUM   },
    { "chr",   TOK_CHR   },
    { "flt",   TOK_FLT   },
    { "blo",   TOK_BLO   },
    { "dob",   TOK_DOB   },
    { "byt",   TOK_BYT   },
    /* statement keywords */
    { "make",  TOK_MAKE  },
    { "ret",   TOK_RET   },
    { "if",    TOK_IF    },
    { "elif",  TOK_ELIF  },
    { "else",  TOK_ELSE  },
    { "loop",  TOK_LOOP  },
    { "break", TOK_BREAK },
    { "when",  TOK_WHEN  },
    { "shape", TOK_SHAPE },
    { "fix",   TOK_FIX   },
    { "mut",   TOK_MUT   },
    { "load",  TOK_LOAD  },
    { "pub",   TOK_PUB   },
    { "as",    TOK_AS    },
    { "in",    TOK_IN    },
    { "and",   TOK_AND   },
    { "or",    TOK_OR    },
    { "not",   TOK_NOT   },
    { "true",  TOK_TRUE  },
    { "false", TOK_FALSE },
    { "sys",   TOK_SYS   },
    { "call",  TOK_CALL  },
    { "get",   TOK_GET   },
    { "safe",  TOK_SAFE  },
    { "fail",  TOK_FAIL  },
    { "match", TOK_MATCH },
    { "case",  TOK_CASE  },
    { "continue", TOK_CONTINUE },
    /* Error handling */
    { "ok",      TOK_OK     },
    { "err",     TOK_ERR    },
    { "some",    TOK_SOME   },
    { "none",    TOK_NONE   },
    { "fn",      TOK_MAKE   },
    { "return",  TOK_RET    },
    { "blueprint", TOK_SHAPE },

    { "extern",  TOK_EXTERN },
    { NULL, 0 }
};

static TokenType lx_lookup_kw(const char *word) {
    for (int i = 0; kw_table[i].word != NULL; i++)
        if (my_strcmp(word, kw_table[i].word) == 0)
            return kw_table[i].type;
    return TOK_IDENT;
}

/* ============================================================ §5 Error */
static void lx_error(Lexer *L, int line, int col, int len, const char *msg) {
    L->had_error = 1;
    rdx_diag_error(line, col, len, "LEX ERROR", msg);
}

/* ============================================================ §6 Emit */
static void lx_emit(Lexer *L, TokenType type, const char *val, int line, int col) {
    if (L->token_count >= (int)MAX_TOKENS) { lx_error(L, line, col, 1, "token buffer overflow"); return; }
    Token *t = &L->tokens[L->token_count++];
    t->type = type; t->line = line; t->col = col;
    my_strncpy(t->value, val, 255); t->value[255] = '\0';
}

/* ============================================================ §7 Scanners */
static void lx_scan_ident(Lexer *L) {
    int start_line = L->line;
    usize start = L->pos;
    int start_col = (int)(start - L->line_start_pos + 1);
    while (L->pos < L->length && lx_is_alnum(lx_peek(L))) L->pos++;
    usize len = L->pos - start; if (len > 255) len = 255;
    char word[256]; my_memcpy(word, L->source + start, len); word[len] = '\0';
    lx_emit(L, lx_lookup_kw(word), word, start_line, start_col);
}

static void lx_scan_number(Lexer *L) {
    int start_line = L->line;
    usize start = L->pos;
    int start_col = (int)(start - L->line_start_pos + 1);
    bool is_float = FALSE;
    while (L->pos < L->length) {
        char c = lx_peek(L);
        if (lx_is_digit(c)) { L->pos++; }
        else if (c == '.' && lx_peek2(L) != '.') {
            /* single dot followed by digit = float; '..' = range op, not consumed here */
            if (lx_is_digit(lx_peek2(L))) { is_float = TRUE; L->pos++; }
            else break;
        } else break;
    }
    usize len = L->pos - start; if (len > 63) len = 63;
    char buf[64]; my_memcpy(buf, L->source + start, len); buf[len] = '\0';
    lx_emit(L, is_float ? TOK_FLT_LIT : TOK_NUM_LIT, buf, start_line, start_col);
}

static void lx_scan_string(Lexer *L) {
    int start_line = L->line;
    usize start = L->pos;
    int start_col = (int)(start - L->line_start_pos + 1);
    L->pos++; /* consume '"' */
    char buf[4097]; usize out = 0;
    while (L->pos < L->length) {
        char c = L->source[L->pos];
        if (c == '"') { L->pos++; break; }
        if (c == '\n' || c == '\r') { lx_error(L, start_line, start_col, 1, "unterminated string"); break; }
        if (c == '\\') {
            L->pos++;
            if (L->pos >= L->length) break;
            char esc = L->source[L->pos++];
            switch (esc) {
                case 'n': buf[out++] = '\n'; break; case 'r': buf[out++] = '\r'; break;
                case 't': buf[out++] = '\t'; break; case '0': buf[out++] = '\0'; break;
                case '\\': buf[out++] = '\\'; break; case '"': buf[out++] = '"'; break;
                default: buf[out++] = '\\'; buf[out++] = esc; break;
            }
        } else { buf[out++] = c; L->pos++; }
        if (out >= 4096) { lx_error(L, start_line, start_col, 1, "string literal too long"); break; }
    }
    buf[out] = '\0';
    lx_emit(L, TOK_STR_LIT, buf, start_line, start_col);
}

/* ============================================================ §8 Main dispatch */
static void lx_next(Lexer *L) {
    lx_skip(L);
    if (L->pos >= L->length) { lx_emit(L, TOK_EOF, "", L->line, (int)(L->pos - L->line_start_pos + 1)); return; }
    int cur_line = L->line;
    int cur_col = (int)(L->pos - L->line_start_pos + 1);
    char c = lx_peek(L);
    if (lx_is_alpha(c)) { lx_scan_ident(L);  return; }
    if (lx_is_digit(c)) { lx_scan_number(L); return; }
    if (c == '"')        { lx_scan_string(L); return; }
    lx_advance(L);
    switch (c) {
        case '+': lx_emit(L, TOK_PLUS,     "+",  cur_line, cur_col); break;
        case '*': lx_emit(L, TOK_STAR,     "*",  cur_line, cur_col); break;
        case '%': lx_emit(L, TOK_PERCENT,  "%",  cur_line, cur_col); break;
        case '(': lx_emit(L, TOK_LPAREN,   "(",  cur_line, cur_col); break;
        case ')': lx_emit(L, TOK_RPAREN,   ")",  cur_line, cur_col); break;
        case '{': lx_emit(L, TOK_LBRACE,   "{",  cur_line, cur_col); break;
        case '}': lx_emit(L, TOK_RBRACE,   "}",  cur_line, cur_col); break;
        case '[': lx_emit(L, TOK_LBRACKET, "[",  cur_line, cur_col); break;
        case ']': lx_emit(L, TOK_RBRACKET, "]",  cur_line, cur_col); break;
        case ';': lx_emit(L, TOK_SEMICOLON,";",  cur_line, cur_col); break;
        case ',': lx_emit(L, TOK_COMMA,    ",",  cur_line, cur_col); break;
        case ':': lx_emit(L, TOK_COLON,    ":",  cur_line, cur_col); break;
        case '/': lx_emit(L, TOK_SLASH,    "/",  cur_line, cur_col); break;
        case '.':
            if (lx_peek(L) == '.') { lx_advance(L); lx_emit(L, TOK_DOTDOT, "..", cur_line, cur_col); }
            else                   { lx_emit(L, TOK_DOT, ".", cur_line, cur_col); }
            break;
        case '-':
            if (lx_peek(L) == '>') { lx_advance(L); lx_emit(L, TOK_ARROW, "->", cur_line, cur_col); }
            else                   { lx_emit(L, TOK_MINUS, "-", cur_line, cur_col); }
            break;
        case '=':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_EQ, "==", cur_line, cur_col); }
            else                   { lx_emit(L, TOK_ASSIGN, "=", cur_line, cur_col); }
            break;
        case '!':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_NEQ, "!=", cur_line, cur_col); }
            else { lx_error(L, cur_line, cur_col, 1, "unexpected '!'"); lx_emit(L, TOK_UNKNOWN, "!", cur_line, cur_col); }
            break;
        case '<':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_LTE, "<=", cur_line, cur_col); }
            else                   { lx_emit(L, TOK_LT, "<", cur_line, cur_col); }
            break;
        case '>':
            if (lx_peek(L) == '=') { lx_advance(L); lx_emit(L, TOK_GTE, ">=", cur_line, cur_col); }
            else                   { lx_emit(L, TOK_GT, ">", cur_line, cur_col); }
            break;
        case '?':
            lx_emit(L, TOK_QUESTION, "?", cur_line, cur_col);
            break;
        default: {
            char msg[48] = "unexpected character: '?'"; msg[23] = c;
            lx_error(L, cur_line, cur_col, 1, msg);
            char unk[2] = { c, '\0' }; lx_emit(L, TOK_UNKNOWN, unk, cur_line, cur_col);
            break;
        }
    }
}

/* ============================================================ §9 Public API */
void lexer_init(Lexer *L, const char *source) {
    L->source = source; L->pos = 0; L->length = my_strlen(source);
    L->line = 1; L->line_start_pos = 0; L->token_count = 0; L->had_error = 0;
    L->tokens = (Token*)rdx_alloc_bump(sizeof(Token) * MAX_TOKENS);
    if (L->tokens == NULL) rdx_panic("lexer_init: OOM");
}
int lexer_tokenize(Lexer *L) {
    while (1) {
        lx_next(L);
        if (L->token_count > 0 && L->tokens[L->token_count - 1].type == TOK_EOF) break;
        if (L->token_count >= (int)MAX_TOKENS) break;
    }
    return L->had_error ? -1 : L->token_count;
}

const char *token_type_name(TokenType t) {
    static const char *names[] = {
        "NUM_LIT","STR_LIT","FLT_LIT",
        "num","chr","flt","blo","dob","byt",
        "make","ret","if","elif","else","loop","break","when","shape","fix","mut",
        "load","pub","as","in","and","or","not","true","false",
        "alloc","drop","sys","call","get","safe","fail","match","case","continue",
        "ok","err","some","none","?","extern",
        "IDENT",
        "+","-","*","/","%",
        "==","!=","<",">","<=",">=",
        "=","->","..",
        "(",")","{","}","[","]",";",",",".",":",
        "EOF","UNKNOWN",NULL
    };
    if ((int)t < 0 || t >= TOK_COUNT) return "???";
    return names[t];
}
void lexer_dump(const Lexer *L) {
    rdx_print("=== Tokens ===\r\n");
    for (int i = 0; i < L->token_count; i++) {
        const Token *t = &L->tokens[i];
        rdx_print("  ["); rdx_print_num((i64)i); rdx_print("] L");
        rdx_print_num((i64)t->line); rdx_print("  ");
        rdx_print(token_type_name(t->type)); rdx_print("  \"");
        rdx_print(t->value); rdx_print("\"\r\n");
    }
    rdx_print("=== "); rdx_print_num((i64)L->token_count); rdx_print(" tokens ===\r\n");
}
