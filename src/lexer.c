/*
 * ============================================================================
 *  lexer.c  —  RDX Hand-Rolled Lexical Analyzer Implementation
 * ============================================================================
 */

#include "lexer.h"

const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_EOF:          return "EOF";
        case TOK_ERROR:        return "ERROR";
        case TOK_INT_LIT:      return "INT_LIT";
        case TOK_FLOAT_LIT:    return "FLOAT_LIT";
        case TOK_STR_LIT:      return "STR_LIT";
        case TOK_CHAR_LIT:     return "CHAR_LIT";
        case TOK_TRUE:         return "true";
        case TOK_FALSE:        return "false";
        case TOK_NIL:          return "nil";
        case TOK_FN:           return "fn";
        case TOK_LET:          return "let";
        case TOK_MUT:          return "mut";
        case TOK_FIX:          return "fix";
        case TOK_IF:           return "if";
        case TOK_ELIF:         return "elif";
        case TOK_ELSE:         return "else";
        case TOK_LOOP:         return "loop";
        case TOK_WHILE:        return "while";
        case TOK_FOR:          return "for";
        case TOK_IN:           return "in";
        case TOK_BREAK:        return "break";
        case TOK_CONTINUE:     return "continue";
        case TOK_RETURN:       return "return";
        case TOK_YIELD:        return "yield";
        case TOK_SPAWN:        return "spawn";
        case TOK_CAP:          return "cap";
        case TOK_SHAPE:        return "shape";
        case TOK_IMPL:         return "impl";
        case TOK_SELF:         return "self";
        case TOK_PUB:          return "pub";
        case TOK_LOAD:         return "load";
        case TOK_EXTERN:       return "extern";
        case TOK_PRINT:        return "print";
        case TOK_VOLATILE:     return "volatile";
        case TOK_REG:          return "reg";
        case TOK_PACKED:       return "packed";
        case TOK_STRUCT:       return "struct";
        case TOK_COMPTIME:     return "comptime";
        case TOK_ASM:          return "asm";
        case TOK_ISR:          return "isr";
        case TOK_CAPABILITY:   return "capability";
        case TOK_U8:           return "u8";
        case TOK_U16:          return "u16";
        case TOK_U32:          return "u32";
        case TOK_U64:          return "u64";
        case TOK_I8:           return "i8";
        case TOK_I16:          return "i16";
        case TOK_I32:          return "i32";
        case TOK_I64:          return "i64";
        case TOK_USIZE:        return "usize";
        case TOK_ISIZE:        return "isize";
        case TOK_F32:          return "f32";
        case TOK_F64:          return "f64";
        case TOK_TY_INT:       return "int";
        case TOK_TY_FLOAT:     return "float";
        case TOK_TY_BOOL:      return "bool";
        case TOK_TY_STR:       return "str";
        case TOK_TY_BYTE:      return "byte";
        case TOK_TY_VOID:      return "void";
        case TOK_AT:           return "@";
        case TOK_IDENT:        return "IDENTIFIER";
        case TOK_PLUS:         return "+";
        case TOK_MINUS:        return "-";
        case TOK_STAR:         return "*";
        case TOK_SLASH:        return "/";
        case TOK_PERCENT:      return "%";
        case TOK_ASSIGN:       return "=";
        case TOK_PLUS_ASSIGN:  return "+=";
        case TOK_MINUS_ASSIGN: return "-=";
        case TOK_STAR_ASSIGN:  return "*=";
        case TOK_SLASH_ASSIGN: return "/=";
        case TOK_EQ:           return "==";
        case TOK_NEQ:          return "!=";
        case TOK_LT:           return "<";
        case TOK_LTE:          return "<=";
        case TOK_GT:           return ">";
        case TOK_GTE:          return ">=";
        case TOK_AND:          return "and";
        case TOK_OR:           return "or";
        case TOK_NOT:          return "not";
        case TOK_AMP:          return "&";
        case TOK_PIPE:         return "|";
        case TOK_CARET:        return "^";
        case TOK_TILDE:        return "~";
        case TOK_SHL:          return "<<";
        case TOK_SHR:          return ">>";
        case TOK_LPAREN:       return "(";
        case TOK_RPAREN:       return ")";
        case TOK_LBRACE:       return "{";
        case TOK_RBRACE:       return "}";
        case TOK_LBRACKET:     return "[";
        case TOK_RBRACKET:     return "]";
        case TOK_COMMA:        return ",";
        case TOK_DOT:          return ".";
        case TOK_DOTDOT:       return "..";
        case TOK_COLON:        return ":";
        case TOK_SEMICOLON:    return ";";
        case TOK_ARROW:        return "->";
        case TOK_FAT_ARROW:    return "=>";
        case TOK_QUESTION:     return "?";
        default:               return "<unknown>";
    }
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

static inline bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

void lexer_init(Lexer *lexer, const char *source, usize length, const char *filename) {
    lexer->source     = source;
    lexer->cursor     = source;
    lexer->start      = source;
    lexer->source_len = length;
    lexer->line       = 1;
    lexer->col        = 1;
    lexer->line_start = source;
    lexer->filename   = filename ? filename : "<input>";
}

static inline bool is_at_end(const Lexer *lexer) {
    return (usize)(lexer->cursor - lexer->source) >= lexer->source_len || *lexer->cursor == '\0';
}

static inline char peek(const Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    return *lexer->cursor;
}

static inline char peek_next(const Lexer *lexer) {
    if ((usize)(lexer->cursor - lexer->source) + 1 >= lexer->source_len) return '\0';
    return lexer->cursor[1];
}

static inline char advance(Lexer *lexer) {
    if (is_at_end(lexer)) return '\0';
    char c = *lexer->cursor++;
    if (c == '\n') {
        lexer->line++;
        lexer->col = 1;
        lexer->line_start = lexer->cursor;
    } else {
        lexer->col++;
    }
    return c;
}

static inline bool match(Lexer *lexer, char expected) {
    if (is_at_end(lexer) || *lexer->cursor != expected) return FALSE;
    advance(lexer);
    return TRUE;
}

static void skip_whitespace_and_comments(Lexer *lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lexer);
        } else if (c == '#' || (c == '/' && peek_next(lexer) == '/')) {
            /* Single-line comment */
            while (!is_at_end(lexer) && peek(lexer) != '\n') {
                advance(lexer);
            }
        } else if (c == '/' && peek_next(lexer) == '*') {
            /* Multi-line comment */
            advance(lexer); /* '/' */
            advance(lexer); /* '*' */
            while (!is_at_end(lexer)) {
                if (peek(lexer) == '*' && peek_next(lexer) == '/') {
                    advance(lexer);
                    advance(lexer);
                    break;
                }
                advance(lexer);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer *lexer, TokenType type) {
    Token tok;
    tok.type   = type;
    tok.start  = lexer->start;
    tok.length = (usize)(lexer->cursor - lexer->start);
    tok.line   = lexer->line;
    tok.col    = (u32)(lexer->start - lexer->line_start + 1);
    tok.int_val = 0;
    return tok;
}

static Token make_error_token(Lexer *lexer, const char *message) {
    Token tok;
    tok.type    = TOK_ERROR;
    tok.start   = message;
    tok.length  = my_strlen(message);
    tok.line    = lexer->line;
    tok.col     = (u32)(lexer->start - lexer->line_start + 1);
    tok.int_val = 0;
    return tok;
}

/* Fast Trie / Keyword Recognition */
static TokenType check_keyword(const char *start, usize length, const char *rest, TokenType type) {
    usize rest_len = my_strlen(rest);
    if (length == rest_len + (usize)(start - start) && my_strncmp(start, rest, rest_len) == 0) {
        return type;
    }
    return TOK_IDENT;
}

static TokenType identifier_type(const char *start, usize length) {
    #define MATCH_KW(str, tok_type) \
        if (length == sizeof(str)-1 && my_strncmp(start, str, sizeof(str)-1) == 0) return tok_type

    switch (start[0]) {
        case 'a':
            MATCH_KW("and", TOK_AND);
            MATCH_KW("as", TOK_IN);
            MATCH_KW("asm", TOK_ASM);
            break;
        case 'b':
            MATCH_KW("bool", TOK_TY_BOOL);
            MATCH_KW("blo", TOK_TY_BOOL);
            MATCH_KW("byte", TOK_TY_BYTE);
            MATCH_KW("byt", TOK_TY_BYTE);
            MATCH_KW("break", TOK_BREAK);
            break;
        case 'c':
            MATCH_KW("cap", TOK_CAP);
            MATCH_KW("capability", TOK_CAPABILITY);
            MATCH_KW("comptime", TOK_COMPTIME);
            MATCH_KW("call", TOK_PRINT);
            MATCH_KW("chr", TOK_TY_STR);
            MATCH_KW("continue", TOK_CONTINUE);
            MATCH_KW("case", TOK_IDENT);
            break;
        case 'd':
            MATCH_KW("dob", TOK_TY_FLOAT);
            MATCH_KW("drop", TOK_IDENT);
            break;
        case 'e':
            MATCH_KW("else", TOK_ELSE);
            MATCH_KW("elif", TOK_ELIF);
            MATCH_KW("extern", TOK_EXTERN);
            break;
        case 'f':
            MATCH_KW("fn", TOK_FN);
            MATCH_KW("fix", TOK_FIX);
            MATCH_KW("for", TOK_FOR);
            MATCH_KW("float", TOK_TY_FLOAT);
            MATCH_KW("flt", TOK_TY_FLOAT);
            MATCH_KW("false", TOK_FALSE);
            MATCH_KW("f32", TOK_F32);
            MATCH_KW("f64", TOK_F64);
            break;
        case 'i':
            MATCH_KW("if", TOK_IF);
            MATCH_KW("in", TOK_IN);
            MATCH_KW("int", TOK_TY_INT);
            MATCH_KW("impl", TOK_IMPL);
            MATCH_KW("isr", TOK_ISR);
            MATCH_KW("i8", TOK_I8);
            MATCH_KW("i16", TOK_I16);
            MATCH_KW("i32", TOK_I32);
            MATCH_KW("i64", TOK_I64);
            MATCH_KW("isize", TOK_ISIZE);
            break;
        case 'l':
            MATCH_KW("let", TOK_LET);
            MATCH_KW("loop", TOK_LOOP);
            MATCH_KW("load", TOK_LOAD);
            break;
        case 'm':
            MATCH_KW("mut", TOK_MUT);
            MATCH_KW("make", TOK_FN);
            MATCH_KW("match", TOK_IDENT);
            break;
        case 'n':
            MATCH_KW("nil", TOK_NIL);
            MATCH_KW("none", TOK_NIL);
            MATCH_KW("not", TOK_NOT);
            MATCH_KW("num", TOK_TY_INT);
            break;
        case 'o':
            MATCH_KW("or", TOK_OR);
            MATCH_KW("ok", TOK_IDENT);
            break;
        case 'p':
            MATCH_KW("print", TOK_PRINT);
            MATCH_KW("pub", TOK_PUB);
            MATCH_KW("packed", TOK_PACKED);
            break;
        case 'r':
            MATCH_KW("return", TOK_RETURN);
            MATCH_KW("ret", TOK_RETURN);
            MATCH_KW("reg", TOK_REG);
            break;
        case 's':
            MATCH_KW("spawn", TOK_SPAWN);
            MATCH_KW("self", TOK_SELF);
            MATCH_KW("shape", TOK_SHAPE);
            MATCH_KW("struct", TOK_STRUCT);
            MATCH_KW("str", TOK_TY_STR);
            MATCH_KW("sys", TOK_CAP);
            break;
        case 't':
            MATCH_KW("true", TOK_TRUE);
            break;
        case 'u':
            MATCH_KW("u8", TOK_U8);
            MATCH_KW("u16", TOK_U16);
            MATCH_KW("u32", TOK_U32);
            MATCH_KW("u64", TOK_U64);
            MATCH_KW("usize", TOK_USIZE);
            break;
        case 'v':
            MATCH_KW("void", TOK_TY_VOID);
            MATCH_KW("volatile", TOK_VOLATILE);
            break;
        case 'w':
            MATCH_KW("while", TOK_WHILE);
            MATCH_KW("when", TOK_IDENT);
            break;
        case 'y':
            MATCH_KW("yield", TOK_YIELD);
            break;
    }
    #undef MATCH_KW
    return TOK_IDENT;
}

static Token scan_identifier(Lexer *lexer) {
    while (is_alnum(peek(lexer))) {
        advance(lexer);
    }
    usize length = (usize)(lexer->cursor - lexer->start);
    TokenType type = identifier_type(lexer->start, length);
    return make_token(lexer, type);
}

static Token scan_number(Lexer *lexer) {
    bool is_float = FALSE;
    const char *start = lexer->start;

    /* Hexadecimal 0x... */
    if (start[0] == '0' && (peek(lexer) == 'x' || peek(lexer) == 'X')) {
        advance(lexer); /* 'x' */
        i64 val = 0;
        while (is_hex_digit(peek(lexer))) {
            char c = advance(lexer);
            val = (val << 4);
            if (c >= '0' && c <= '9') val += (c - '0');
            else if (c >= 'a' && c <= 'f') val += (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val += (c - 'A' + 10);
        }
        Token tok = make_token(lexer, TOK_INT_LIT);
        tok.int_val = val;
        return tok;
    }

    /* Binary 0b... */
    if (start[0] == '0' && (peek(lexer) == 'b' || peek(lexer) == 'B')) {
        advance(lexer); /* 'b' */
        i64 val = 0;
        while (peek(lexer) == '0' || peek(lexer) == '1') {
            char c = advance(lexer);
            val = (val << 1) + (c - '0');
        }
        Token tok = make_token(lexer, TOK_INT_LIT);
        tok.int_val = val;
        return tok;
    }

    /* Decimal */
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }

    /* Fractional */
    if (peek(lexer) == '.' && is_digit(peek_next(lexer))) {
        is_float = TRUE;
        advance(lexer); /* '.' */
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    /* Scientific exponent (e/E) */
    if (peek(lexer) == 'e' || peek(lexer) == 'E') {
        is_float = TRUE;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-') {
            advance(lexer);
        }
        while (is_digit(peek(lexer))) {
            advance(lexer);
        }
    }

    Token tok = make_token(lexer, is_float ? TOK_FLOAT_LIT : TOK_INT_LIT);

    if (is_float) {
        /* Parse double */
        double result = 0.0;
        double sign = 1.0;
        const char *p = tok.start;
        if (*p == '-') { sign = -1.0; p++; }
        else if (*p == '+') { p++; }

        while (is_digit(*p)) {
            result = result * 10.0 + (*p - '0');
            p++;
        }
        if (*p == '.') {
            p++;
            double factor = 0.1;
            while (is_digit(*p)) {
                result += (*p - '0') * factor;
                factor *= 0.1;
                p++;
            }
        }
        tok.float_val = result * sign;
    } else {
        /* Parse i64 */
        i64 val = 0;
        const char *p = tok.start;
        while (p < lexer->cursor && is_digit(*p)) {
            val = val * 10 + (*p - '0');
            p++;
        }
        tok.int_val = val;
    }

    return tok;
}

static Token scan_string(Lexer *lexer) {
    while (!is_at_end(lexer) && peek(lexer) != '"') {
        if (peek(lexer) == '\\') {
            advance(lexer); /* skip escape char */
        }
        advance(lexer);
    }

    if (is_at_end(lexer)) {
        return make_error_token(lexer, "Unterminated string literal");
    }

    advance(lexer); /* closing '"' */
    return make_token(lexer, TOK_STR_LIT);
}

static Token scan_char(Lexer *lexer) {
    if (is_at_end(lexer) || peek(lexer) == '\'') {
        return make_error_token(lexer, "Empty character literal");
    }

    char val = advance(lexer);
    if (val == '\\') {
        char esc = advance(lexer);
        switch (esc) {
            case 'n': val = '\n'; break;
            case 't': val = '\t'; break;
            case 'r': val = '\r'; break;
            case '\\': val = '\\'; break;
            case '\'': val = '\''; break;
            case '0': val = '\0'; break;
            default: val = esc; break;
        }
    }

    if (!match(lexer, '\'')) {
        return make_error_token(lexer, "Unterminated character literal");
    }

    Token tok = make_token(lexer, TOK_CHAR_LIT);
    tok.char_val = val;
    return tok;
}

Token lexer_next(Lexer *lexer) {
    skip_whitespace_and_comments(lexer);
    lexer->start = lexer->cursor;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOK_EOF);
    }

    char c = advance(lexer);

    if (is_alpha(c)) return scan_identifier(lexer);
    if (is_digit(c)) return scan_number(lexer);

    switch (c) {
        case '(': return make_token(lexer, TOK_LPAREN);
        case ')': return make_token(lexer, TOK_RPAREN);
        case '{': return make_token(lexer, TOK_LBRACE);
        case '}': return make_token(lexer, TOK_RBRACE);
        case '[': return make_token(lexer, TOK_LBRACKET);
        case ']': return make_token(lexer, TOK_RBRACKET);
        case ',': return make_token(lexer, TOK_COMMA);
        case ';': return make_token(lexer, TOK_SEMICOLON);
        case ':': return make_token(lexer, TOK_COLON);
        case '?': return make_token(lexer, TOK_QUESTION);
        case '@': return make_token(lexer, TOK_AT);
        case '~': return make_token(lexer, TOK_TILDE);
        case '^': return make_token(lexer, TOK_CARET);

        case '.':
            if (match(lexer, '.')) return make_token(lexer, TOK_DOTDOT);
            return make_token(lexer, TOK_DOT);

        case '+':
            if (match(lexer, '=')) return make_token(lexer, TOK_PLUS_ASSIGN);
            return make_token(lexer, TOK_PLUS);

        case '-':
            if (match(lexer, '>')) return make_token(lexer, TOK_ARROW);
            if (match(lexer, '=')) return make_token(lexer, TOK_MINUS_ASSIGN);
            return make_token(lexer, TOK_MINUS);

        case '*':
            if (match(lexer, '=')) return make_token(lexer, TOK_STAR_ASSIGN);
            return make_token(lexer, TOK_STAR);

        case '/':
            if (match(lexer, '=')) return make_token(lexer, TOK_SLASH_ASSIGN);
            return make_token(lexer, TOK_SLASH);

        case '%': return make_token(lexer, TOK_PERCENT);

        case '!':
            if (match(lexer, '=')) return make_token(lexer, TOK_NEQ);
            return make_token(lexer, TOK_NOT);

        case '=':
            if (match(lexer, '=')) return make_token(lexer, TOK_EQ);
            if (match(lexer, '>')) return make_token(lexer, TOK_FAT_ARROW);
            return make_token(lexer, TOK_ASSIGN);

        case '<':
            if (match(lexer, '<')) return make_token(lexer, TOK_SHL);
            if (match(lexer, '=')) return make_token(lexer, TOK_LTE);
            return make_token(lexer, TOK_LT);

        case '>':
            if (match(lexer, '>')) return make_token(lexer, TOK_SHR);
            if (match(lexer, '=')) return make_token(lexer, TOK_GTE);
            return make_token(lexer, TOK_GT);

        case '&':
            if (match(lexer, '&')) return make_token(lexer, TOK_AND);
            return make_token(lexer, TOK_AMP);

        case '|':
            if (match(lexer, '|')) return make_token(lexer, TOK_OR);
            return make_token(lexer, TOK_PIPE);

        case '"': return scan_string(lexer);
        case '\'': return scan_char(lexer);

        default:
            return make_error_token(lexer, "Unexpected character encountered");
    }
}

Token lexer_peek(Lexer *lexer) {
    Lexer save = *lexer;
    Token tok = lexer_next(lexer);
    *lexer = save;
    return tok;
}
