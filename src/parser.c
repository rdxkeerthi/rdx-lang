/*
 * ============================================================================
 *  parser.c  —  RDX Pratt Parser Implementation
 * ============================================================================
 */

#include "parser.h"

/* Forward Declarations */
static AstExpr *parse_prefix(Parser *parser, bool can_assign);
static AstExpr *parse_infix(Parser *parser, AstExpr *left, bool can_assign);
static Precedence get_infix_precedence(TokenType type);

/* Error handling */
static void error_at(Parser *parser, Token *tok, const char *msg) {
    if (parser->panic_mode) return;
    parser->panic_mode = TRUE;
    parser->had_error  = TRUE;
    parser->error_msg  = msg;

    rdx_print("[ERROR] Line ");
    rdx_print_num(tok->line);
    rdx_print(" Col ");
    rdx_print_num(tok->col);
    rdx_print(": ");
    rdx_print(msg);
    rdx_print("\n");
}

static void error_current(Parser *parser, const char *msg) {
    error_at(parser, &parser->current, msg);
}

static void advance_parser(Parser *parser) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = lexer_next(&parser->lexer);
        if (parser->current.type != TOK_ERROR) break;
        error_current(parser, parser->current.start);
    }
}

static bool check(const Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool match_token(Parser *parser, TokenType type) {
    if (!check(parser, type)) return FALSE;
    advance_parser(parser);
    return TRUE;
}

static void consume(Parser *parser, TokenType type, const char *msg) {
    if (parser->current.type == type) {
        advance_parser(parser);
        return;
    }
    error_current(parser, msg);
}

static void synchronize(Parser *parser) {
    parser->panic_mode = FALSE;
    while (parser->current.type != TOK_EOF) {
        if (parser->previous.type == TOK_SEMICOLON) return;
        switch (parser->current.type) {
            case TOK_FN:
            case TOK_LET:
            case TOK_MUT:
            case TOK_FIX:
            case TOK_IF:
            case TOK_WHILE:
            case TOK_LOOP:
            case TOK_FOR:
            case TOK_RETURN:
            case TOK_SHAPE:
            case TOK_PRINT:
                return;
            default:
                break;
        }
        advance_parser(parser);
    }
}

void parser_init(Parser *parser, const char *source, usize length, const char *filename, Arena *arena) {
    lexer_init(&parser->lexer, source, length, filename);
    parser->arena      = arena;
    parser->had_error  = FALSE;
    parser->panic_mode = FALSE;
    parser->error_msg  = NULL;
    advance_parser(parser);
}

/* ============================================================
 *  AST Node Helpers
 * ============================================================ */

static AstExpr *alloc_expr(Arena *arena, AstExprKind kind, u32 line, u32 col) {
    AstExpr *node = (AstExpr*)arena_alloc_zero(arena, sizeof(AstExpr));
    if (node) {
        node->kind = kind;
        node->line = line;
        node->col  = col;
    }
    return node;
}

static AstStmt *alloc_stmt(Arena *arena, AstStmtKind kind, u32 line, u32 col) {
    AstStmt *node = (AstStmt*)arena_alloc_zero(arena, sizeof(AstStmt));
    if (node) {
        node->kind = kind;
        node->line = line;
        node->col  = col;
    }
    return node;
}

static AstDecl *alloc_decl(Arena *arena, AstDeclKind kind, u32 line, u32 col) {
    AstDecl *node = (AstDecl*)arena_alloc_zero(arena, sizeof(AstDecl));
    if (node) {
        node->kind = kind;
        node->line = line;
        node->col  = col;
    }
    return node;
}

AstExpr *ast_make_int(Arena *arena, i64 val, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_INT, line, col);
    if (node) node->int_val = val;
    return node;
}

AstExpr *ast_make_float(Arena *arena, double val, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_FLOAT, line, col);
    if (node) node->float_val = val;
    return node;
}

AstExpr *ast_make_str(Arena *arena, const char *chars, usize length, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_STR, line, col);
    if (node) {
        node->str_val.chars  = arena_strndup(arena, chars, length);
        node->str_val.length = length;
    }
    return node;
}

AstExpr *ast_make_bool(Arena *arena, bool val, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_BOOL, line, col);
    if (node) node->bool_val = val;
    return node;
}

AstExpr *ast_make_nil(Arena *arena, u32 line, u32 col) {
    return alloc_expr(arena, EXPR_NIL, line, col);
}

AstExpr *ast_make_ident(Arena *arena, const char *name, usize length, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_IDENT, line, col);
    if (node) {
        node->ident.name   = arena_strndup(arena, name, length);
        node->ident.length = length;
    }
    return node;
}

AstExpr *ast_make_unary(Arena *arena, TokenType op, AstExpr *operand, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_UNARY, line, col);
    if (node) {
        node->unary.op      = op;
        node->unary.operand = operand;
    }
    return node;
}

AstExpr *ast_make_binary(Arena *arena, TokenType op, AstExpr *left, AstExpr *right, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_BINARY, line, col);
    if (node) {
        node->binary.op    = op;
        node->binary.left  = left;
        node->binary.right = right;
    }
    return node;
}

AstExpr *ast_make_call(Arena *arena, AstExpr *callee, AstExpr **args, u32 count, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_CALL, line, col);
    if (node) {
        node->call.callee    = callee;
        node->call.args      = args;
        node->call.arg_count = count;
    }
    return node;
}

AstExpr *ast_make_block(Arena *arena, AstStmt **stmts, u32 count, AstExpr *result, u32 line, u32 col) {
    AstExpr *node = alloc_expr(arena, EXPR_BLOCK, line, col);
    if (node) {
        node->block.stmts       = stmts;
        node->block.stmt_count  = count;
        node->block.result_expr = result;
    }
    return node;
}

/* ============================================================
 *  Pratt Parsing Engine
 * ============================================================ */

static Precedence get_infix_precedence(TokenType type) {
    switch (type) {
        case TOK_ASSIGN:
        case TOK_PLUS_ASSIGN:
        case TOK_MINUS_ASSIGN:
        case TOK_STAR_ASSIGN:
        case TOK_SLASH_ASSIGN:
            return PREC_ASSIGNMENT;
        case TOK_OR:
            return PREC_OR;
        case TOK_AND:
            return PREC_AND;
        case TOK_PIPE:
            return PREC_BITWISE_OR;
        case TOK_CARET:
            return PREC_BITWISE_XOR;
        case TOK_AMP:
            return PREC_BITWISE_AND;
        case TOK_EQ:
        case TOK_NEQ:
            return PREC_EQUALITY;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE:
            return PREC_COMPARISON;
        case TOK_SHL:
        case TOK_SHR:
            return PREC_SHIFT;
        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_TERM;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
            return PREC_FACTOR;
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_DOT:
            return PREC_CALL;
        default:
            return PREC_NONE;
    }
}

AstExpr *parser_parse_expr(Parser *parser, Precedence precedence) {
    advance_parser(parser);
    bool can_assign = (precedence <= PREC_ASSIGNMENT);
    AstExpr *expr = parse_prefix(parser, can_assign);
    if (!expr) return NULL;

    while (precedence <= get_infix_precedence(parser->current.type)) {
        advance_parser(parser);
        expr = parse_infix(parser, expr, can_assign);
        if (!expr) break;
    }

    if (can_assign && (parser->current.type == TOK_ASSIGN ||
                       parser->current.type == TOK_PLUS_ASSIGN ||
                       parser->current.type == TOK_MINUS_ASSIGN)) {
        error_current(parser, "Invalid assignment target");
    }

    return expr;
}

static AstExpr *parse_block_expr(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    u32 capacity = 16;
    u32 count = 0;
    AstStmt **stmts = (AstStmt**)arena_alloc(parser->arena, sizeof(AstStmt*) * capacity);
    AstExpr *result_expr = NULL;

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        if (count >= capacity) {
            u32 new_cap = capacity * 2;
            AstStmt **new_stmts = (AstStmt**)arena_alloc(parser->arena, sizeof(AstStmt*) * new_cap);
            my_memcpy(new_stmts, stmts, sizeof(AstStmt*) * capacity);
            stmts = new_stmts;
            capacity = new_cap;
        }

        if (!check(parser, TOK_LET) && !check(parser, TOK_MUT) && !check(parser, TOK_FIX) &&
            !check(parser, TOK_IF) && !check(parser, TOK_WHILE) && !check(parser, TOK_LOOP) &&
            !check(parser, TOK_RETURN) && !check(parser, TOK_BREAK) && !check(parser, TOK_CONTINUE) &&
            !check(parser, TOK_PRINT) && !check(parser, TOK_ASM)) {
            AstExpr *expr = parser_parse_expr(parser, PREC_ASSIGNMENT);
            if (!expr) break;
            if (match_token(parser, TOK_SEMICOLON)) {
                AstStmt *node = alloc_stmt(parser->arena, STMT_EXPR, expr->line, expr->col);
                if (node) node->expr = expr;
                stmts[count++] = node;
            } else if (check(parser, TOK_RBRACE)) {
                result_expr = expr;
                break;
            } else {
                consume(parser, TOK_SEMICOLON, "Expected ';' after expression");
                AstStmt *node = alloc_stmt(parser->arena, STMT_EXPR, expr->line, expr->col);
                if (node) node->expr = expr;
                stmts[count++] = node;
            }
        } else {
            stmts[count++] = parser_parse_stmt(parser);
        }
    }

    consume(parser, TOK_RBRACE, "Expected '}' after block");
    return ast_make_block(parser->arena, stmts, count, result_expr, line, col);
}

static AstExpr *parse_prefix(Parser *parser, bool can_assign) {
    (void)can_assign;
    Token tok = parser->previous;

    switch (tok.type) {
        case TOK_INT_LIT:
            return ast_make_int(parser->arena, tok.int_val, tok.line, tok.col);
        case TOK_FLOAT_LIT:
            return ast_make_float(parser->arena, tok.float_val, tok.line, tok.col);
        case TOK_STR_LIT: {
            /* Strip surrounding quotes */
            const char *str_chars = tok.start + 1;
            usize str_len = (tok.length >= 2) ? (tok.length - 2) : 0;
            return ast_make_str(parser->arena, str_chars, str_len, tok.line, tok.col);
        }
        case TOK_TRUE:
            return ast_make_bool(parser->arena, TRUE, tok.line, tok.col);
        case TOK_FALSE:
            return ast_make_bool(parser->arena, FALSE, tok.line, tok.col);
        case TOK_NIL:
            return ast_make_nil(parser->arena, tok.line, tok.col);
        case TOK_IDENT:
        case TOK_SELF:
        case TOK_TY_BYTE:
        case TOK_TY_INT:
        case TOK_TY_FLOAT:
        case TOK_TY_BOOL:
        case TOK_TY_STR:
        case TOK_U8:
        case TOK_U16:
        case TOK_U32:
        case TOK_U64:
        case TOK_I8:
        case TOK_I16:
        case TOK_I32:
        case TOK_I64:
        case TOK_USIZE:
        case TOK_ISIZE:
        case TOK_F32:
        case TOK_F64:
            return ast_make_ident(parser->arena, tok.start, tok.length, tok.line, tok.col);

        case TOK_MINUS:
        case TOK_NOT:
        case TOK_TILDE: {
            AstExpr *operand = parser_parse_expr(parser, PREC_UNARY);
            return ast_make_unary(parser->arena, tok.type, operand, tok.line, tok.col);
        }

        case TOK_LPAREN: {
            AstExpr *expr = parser_parse_expr(parser, PREC_ASSIGNMENT);
            consume(parser, TOK_RPAREN, "Expected ')' after expression");
            return expr;
        }

        case TOK_LBRACE:
            return parse_block_expr(parser);

        case TOK_LBRACKET: {
            /* Array literal [elem1, elem2, ...] */
            u32 line = tok.line;
            u32 col  = tok.col;
            u32 cap  = 8;
            u32 count = 0;
            AstExpr **elems = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * cap);

            if (!check(parser, TOK_RBRACKET)) {
                do {
                    if (count >= cap) {
                        u32 new_cap = cap * 2;
                        AstExpr **new_elems = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * new_cap);
                        my_memcpy(new_elems, elems, sizeof(AstExpr*) * cap);
                        elems = new_elems;
                        cap = new_cap;
                    }
                    elems[count++] = parser_parse_expr(parser, PREC_ASSIGNMENT);
                } while (match_token(parser, TOK_COMMA));
            }
            consume(parser, TOK_RBRACKET, "Expected ']' after array literal");

            AstExpr *arr_node = alloc_expr(parser->arena, EXPR_ARRAY, line, col);
            if (arr_node) {
                arr_node->array.elements = elems;
                arr_node->array.count    = count;
            }
            return arr_node;
        }

        case TOK_SPAWN: {
            /* Structured Concurrency: spawn fiber */
            u32 line = tok.line;
            u32 col  = tok.col;
            AstExpr *call_expr = parser_parse_expr(parser, PREC_UNARY);
            AstExpr *node = alloc_expr(parser->arena, EXPR_SPAWN, line, col);
            if (node) node->spawn.call_expr = call_expr;
            return node;
        }

        case TOK_YIELD: {
            /* Fiber yield */
            u32 line = tok.line;
            u32 col  = tok.col;
            AstExpr *val = NULL;
            if (!check(parser, TOK_SEMICOLON) && !check(parser, TOK_RBRACE)) {
                val = parser_parse_expr(parser, PREC_ASSIGNMENT);
            }
            AstExpr *node = alloc_expr(parser->arena, EXPR_YIELD, line, col);
            if (node) node->yield.value = val;
            return node;
        }

        case TOK_COMPTIME: {
            /* Compile-time constant expression / block */
            u32 line = tok.line;
            u32 col  = tok.col;
            AstExpr *inner = NULL;
            if (match_token(parser, TOK_LBRACE)) {
                inner = parse_block_expr(parser);
            } else {
                inner = parser_parse_expr(parser, PREC_UNARY);
            }
            AstExpr *node = alloc_expr(parser->arena, EXPR_COMPTIME, line, col);
            if (node) node->comptime_expr.expr = inner;
            return node;
        }

        case TOK_ASM: {
            /* Inline assembly */
            u32 line = tok.line;
            u32 col  = tok.col;
            bool is_vol = match_token(parser, TOK_VOLATILE);
            consume(parser, TOK_LPAREN, "Expected '(' after 'asm'");
            consume(parser, TOK_STR_LIT, "Expected assembly string literal");
            Token asm_tok = parser->previous;
            const char *instr = asm_tok.start + 1;
            usize instr_len = (asm_tok.length >= 2) ? (asm_tok.length - 2) : 0;
            consume(parser, TOK_RPAREN, "Expected ')' after assembly string");

            AstExpr *node = alloc_expr(parser->arena, EXPR_ASM, line, col);
            if (node) {
                node->inline_asm.instruction = arena_strndup(parser->arena, instr, instr_len);
                node->inline_asm.length      = instr_len;
                node->inline_asm.is_volatile = is_vol;
            }
            return node;
        }

        case TOK_CAP:
        case TOK_CAPABILITY: {
            /* Zero-Trust Capability Token expression: cap "network.connect" or cap UART0 */
            u32 line = tok.line;
            u32 col  = tok.col;
            const char *cap_name = "";
            usize cap_len = 0;
            if (match_token(parser, TOK_STR_LIT)) {
                Token cap_tok = parser->previous;
                cap_name = cap_tok.start + 1;
                cap_len  = (cap_tok.length >= 2) ? (cap_tok.length - 2) : 0;
            } else if (match_token(parser, TOK_IDENT)) {
                Token cap_tok = parser->previous;
                cap_name = cap_tok.start;
                cap_len  = cap_tok.length;
            } else {
                /* Standalone 'cap' variable name */
                return ast_make_ident(parser->arena, tok.start, tok.length, line, col);
            }

            AstExpr *node = alloc_expr(parser->arena, EXPR_CAP_REQ, line, col);
            if (node) {
                node->cap_req.cap_name = arena_strndup(parser->arena, cap_name, cap_len);
                node->cap_req.cap_len  = cap_len;
            }
            return node;
        }

        default:
            error_current(parser, "Expected expression");
            return NULL;
    }
}

static AstExpr *parse_infix(Parser *parser, AstExpr *left, bool can_assign) {
    Token op = parser->previous;

    switch (op.type) {
        case TOK_ASSIGN:
        case TOK_PLUS_ASSIGN:
        case TOK_MINUS_ASSIGN:
        case TOK_STAR_ASSIGN:
        case TOK_SLASH_ASSIGN: {
            if (!can_assign) {
                error_at(parser, &op, "Invalid assignment target");
            }
            AstExpr *value = parser_parse_expr(parser, PREC_ASSIGNMENT);
            AstExpr *node = alloc_expr(parser->arena, EXPR_ASSIGN, op.line, op.col);
            if (node) {
                node->assign.op     = op.type;
                node->assign.target = left;
                node->assign.value  = value;
            }
            return node;
        }

        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_EQ:
        case TOK_NEQ:
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE:
        case TOK_AND:
        case TOK_OR:
        case TOK_PIPE:
        case TOK_AMP:
        case TOK_CARET:
        case TOK_SHL:
        case TOK_SHR: {
            Precedence prec = get_infix_precedence(op.type);
            AstExpr *right = parser_parse_expr(parser, (Precedence)(prec + 1));
            return ast_make_binary(parser->arena, op.type, left, right, op.line, op.col);
        }

        case TOK_LPAREN: {
            /* Function call */
            u32 cap = 8;
            u32 count = 0;
            AstExpr **args = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * cap);

            if (!check(parser, TOK_RPAREN)) {
                do {
                    if (count >= cap) {
                        u32 new_cap = cap * 2;
                        AstExpr **new_args = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * new_cap);
                        my_memcpy(new_args, args, sizeof(AstExpr*) * cap);
                        args = new_args;
                        cap = new_cap;
                    }
                    args[count++] = parser_parse_expr(parser, PREC_ASSIGNMENT);
                } while (match_token(parser, TOK_COMMA));
            }
            consume(parser, TOK_RPAREN, "Expected ')' after argument list");
            return ast_make_call(parser->arena, left, args, count, op.line, op.col);
        }

        case TOK_DOT: {
            /* Member access */
            consume(parser, TOK_IDENT, "Expected member name after '.'");
            Token field_tok = parser->previous;
            AstExpr *node = alloc_expr(parser->arena, EXPR_MEMBER, op.line, op.col);
            if (node) {
                node->member.target    = left;
                node->member.field     = arena_strndup(parser->arena, field_tok.start, field_tok.length);
                node->member.field_len = field_tok.length;
            }
            return node;
        }

        case TOK_LBRACKET: {
            /* Array index or Bit-Range [hi..lo] */
            AstExpr *first_expr = parser_parse_expr(parser, PREC_ASSIGNMENT);
            if (match_token(parser, TOK_DOTDOT)) {
                AstExpr *low_expr = parser_parse_expr(parser, PREC_ASSIGNMENT);
                consume(parser, TOK_RBRACKET, "Expected ']' after bit range");
                AstExpr *node = alloc_expr(parser->arena, EXPR_BIT_RANGE, op.line, op.col);
                if (node) {
                    node->bit_range.target   = left;
                    node->bit_range.high_bit = first_expr;
                    node->bit_range.low_bit  = low_expr;
                }
                return node;
            }
            consume(parser, TOK_RBRACKET, "Expected ']' after array index");
            AstExpr *node = alloc_expr(parser->arena, EXPR_INDEX, op.line, op.col);
            if (node) {
                node->index.target = left;
                node->index.index  = first_expr;
            }
            return node;
        }

        default:
            return left;
    }
}

/* ============================================================
 *  Statements & Declarations Parsing
 * ============================================================ */

static inline bool is_ident_token(TokenType type) {
    return (type == TOK_IDENT || type == TOK_CAP || type == TOK_CAPABILITY ||
            type == TOK_MUT || type == TOK_FIX || type == TOK_SELF ||
            type == TOK_REG || type == TOK_VOLATILE || type == TOK_PACKED ||
            type == TOK_U8 || type == TOK_U16 || type == TOK_U32 || type == TOK_U64 ||
            type == TOK_I8 || type == TOK_I16 || type == TOK_I32 || type == TOK_I64 ||
            type == TOK_USIZE || type == TOK_ISIZE || type == TOK_F32 || type == TOK_F64 ||
            type == TOK_TY_BYTE || type == TOK_TY_INT || type == TOK_TY_FLOAT ||
            type == TOK_TY_BOOL || type == TOK_TY_STR || type == TOK_TY_VOID);
}

static AstStmt *parse_var_decl(Parser *parser, bool is_mut, bool is_fix) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    if (match_token(parser, TOK_MUT)) {
        is_mut = TRUE;
    }

    if (is_ident_token(parser->current.type)) {
        advance_parser(parser);
    } else {
        consume(parser, TOK_IDENT, "Expected variable name");
    }
    Token name_tok = parser->previous;

    const char *type_name = NULL;
    usize type_len = 0;
    if (match_token(parser, TOK_COLON)) {
        advance_parser(parser);
        type_name = parser->previous.start;
        type_len  = parser->previous.length;
    }

    AstExpr *init = NULL;
    if (match_token(parser, TOK_ASSIGN)) {
        init = parser_parse_expr(parser, PREC_ASSIGNMENT);
    }

    consume(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");

    AstStmt *node = alloc_stmt(parser->arena, STMT_VAR, line, col);
    if (node) {
        node->var_decl.name      = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->var_decl.name_len  = name_tok.length;
        node->var_decl.type_name = type_name ? arena_strndup(parser->arena, type_name, type_len) : NULL;
        node->var_decl.type_len  = type_len;
        node->var_decl.init      = init;
        node->var_decl.is_mut    = is_mut;
        node->var_decl.is_fix    = is_fix;
    }
    return node;
}

static AstStmt *parse_if_stmt(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    AstExpr *cond = parser_parse_expr(parser, PREC_ASSIGNMENT);

    consume(parser, TOK_LBRACE, "Expected '{' before if-body");
    AstExpr *then_block = parse_block_expr(parser);

    AstStmt *else_branch = NULL;
    if (match_token(parser, TOK_ELSE)) {
        if (match_token(parser, TOK_IF)) {
            else_branch = parse_if_stmt(parser);
        } else {
            consume(parser, TOK_LBRACE, "Expected '{' before else-body");
            AstExpr *else_block = parse_block_expr(parser);
            else_branch = alloc_stmt(parser->arena, STMT_EXPR, line, col);
            if (else_branch) else_branch->expr = else_block;
        }
    }

    AstStmt *node = alloc_stmt(parser->arena, STMT_IF, line, col);
    if (node) {
        node->if_stmt.condition   = cond;
        node->if_stmt.then_branch = alloc_stmt(parser->arena, STMT_EXPR, line, col);
        if (node->if_stmt.then_branch) node->if_stmt.then_branch->expr = then_block;
        node->if_stmt.else_branch = else_branch;
    }
    return node;
}

static AstStmt *parse_while_stmt(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    AstExpr *cond = parser_parse_expr(parser, PREC_ASSIGNMENT);
    consume(parser, TOK_LBRACE, "Expected '{' before while body");
    AstExpr *body_block = parse_block_expr(parser);

    AstStmt *node = alloc_stmt(parser->arena, STMT_WHILE, line, col);
    if (node) {
        node->while_stmt.condition = cond;
        node->while_stmt.body      = alloc_stmt(parser->arena, STMT_EXPR, line, col);
        if (node->while_stmt.body) node->while_stmt.body->expr = body_block;
    }
    return node;
}

static AstStmt *parse_loop_stmt(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_LBRACE, "Expected '{' before loop body");
    AstExpr *body_block = parse_block_expr(parser);

    AstStmt *node = alloc_stmt(parser->arena, STMT_LOOP, line, col);
    if (node) {
        node->loop_stmt.body = alloc_stmt(parser->arena, STMT_EXPR, line, col);
        if (node->loop_stmt.body) node->loop_stmt.body->expr = body_block;
    }
    return node;
}

static AstStmt *parse_print_stmt(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    u32 cap = 4;
    u32 count = 0;
    AstExpr **exprs = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * cap);

    if (!check(parser, TOK_SEMICOLON)) {
        do {
            if (count >= cap) {
                u32 new_cap = cap * 2;
                AstExpr **new_exprs = (AstExpr**)arena_alloc(parser->arena, sizeof(AstExpr*) * new_cap);
                my_memcpy(new_exprs, exprs, sizeof(AstExpr*) * cap);
                exprs = new_exprs;
                cap = new_cap;
            }
            exprs[count++] = parser_parse_expr(parser, PREC_ASSIGNMENT);
        } while (match_token(parser, TOK_COMMA));
    }
    consume(parser, TOK_SEMICOLON, "Expected ';' after print statement");

    AstStmt *node = alloc_stmt(parser->arena, STMT_PRINT, line, col);
    if (node) {
        node->print_stmt.exprs = exprs;
        node->print_stmt.count = count;
    }
    return node;
}

AstStmt *parser_parse_stmt(Parser *parser) {
    if (match_token(parser, TOK_LET)) {
        return parse_var_decl(parser, FALSE, FALSE);
    }
    if (match_token(parser, TOK_MUT)) {
        return parse_var_decl(parser, TRUE, FALSE);
    }
    if (match_token(parser, TOK_FIX)) {
        return parse_var_decl(parser, FALSE, TRUE);
    }
    if (match_token(parser, TOK_IF)) {
        return parse_if_stmt(parser);
    }
    if (match_token(parser, TOK_WHILE)) {
        return parse_while_stmt(parser);
    }
    if (match_token(parser, TOK_LOOP)) {
        return parse_loop_stmt(parser);
    }
    if (match_token(parser, TOK_PRINT)) {
        return parse_print_stmt(parser);
    }
    if (match_token(parser, TOK_RETURN)) {
        u32 line = parser->previous.line;
        u32 col  = parser->previous.col;
        AstExpr *val = NULL;
        if (!check(parser, TOK_SEMICOLON)) {
            val = parser_parse_expr(parser, PREC_ASSIGNMENT);
        }
        consume(parser, TOK_SEMICOLON, "Expected ';' after return");
        AstStmt *node = alloc_stmt(parser->arena, STMT_RETURN, line, col);
        if (node) node->return_stmt.value = val;
        return node;
    }
    if (match_token(parser, TOK_BREAK)) {
        u32 line = parser->previous.line;
        u32 col  = parser->previous.col;
        consume(parser, TOK_SEMICOLON, "Expected ';' after break");
        return alloc_stmt(parser->arena, STMT_BREAK, line, col);
    }
    if (match_token(parser, TOK_CONTINUE)) {
        u32 line = parser->previous.line;
        u32 col  = parser->previous.col;
        consume(parser, TOK_SEMICOLON, "Expected ';' after continue");
        return alloc_stmt(parser->arena, STMT_CONTINUE, line, col);
    }
    if (match_token(parser, TOK_ASM)) {
        u32 line = parser->previous.line;
        u32 col  = parser->previous.col;
        bool is_vol = match_token(parser, TOK_VOLATILE);
        consume(parser, TOK_LPAREN, "Expected '(' after 'asm'");
        consume(parser, TOK_STR_LIT, "Expected assembly string literal");
        Token asm_tok = parser->previous;
        const char *instr = asm_tok.start + 1;
        usize instr_len = (asm_tok.length >= 2) ? (asm_tok.length - 2) : 0;
        consume(parser, TOK_RPAREN, "Expected ')' after assembly string");
        consume(parser, TOK_SEMICOLON, "Expected ';' after asm statement");

        AstStmt *node = alloc_stmt(parser->arena, STMT_ASM, line, col);
        if (node) {
            node->asm_stmt.instruction = arena_strndup(parser->arena, instr, instr_len);
            node->asm_stmt.length      = instr_len;
            node->asm_stmt.is_volatile = is_vol;
        }
        return node;
    }

    /* Expression statement */
    u32 line = parser->current.line;
    u32 col  = parser->current.col;
    AstExpr *expr = parser_parse_expr(parser, PREC_ASSIGNMENT);
    consume(parser, TOK_SEMICOLON, "Expected ';' after expression");

    AstStmt *node = alloc_stmt(parser->arena, STMT_EXPR, line, col);
    if (node) node->expr = expr;
    return node;
}

static AstDecl *parse_fn_decl(Parser *parser, bool is_pub) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_IDENT, "Expected function name");
    Token name_tok = parser->previous;

    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    u32 cap = 4;
    u32 param_count = 0;
    AstParam *params = (AstParam*)arena_alloc(parser->arena, sizeof(AstParam) * cap);

    if (!check(parser, TOK_RPAREN)) {
        do {
            if (param_count >= cap) {
                u32 new_cap = cap * 2;
                AstParam *new_params = (AstParam*)arena_alloc(parser->arena, sizeof(AstParam) * new_cap);
                my_memcpy(new_params, params, sizeof(AstParam) * cap);
                params = new_params;
                cap = new_cap;
            }
            if (is_ident_token(parser->current.type)) {
                advance_parser(parser);
            } else {
                consume(parser, TOK_IDENT, "Expected parameter name");
            }
            Token p_name = parser->previous;

            const char *p_type = NULL;
            usize p_type_len = 0;
            if (match_token(parser, TOK_COLON)) {
                advance_parser(parser);
                p_type     = parser->previous.start;
                p_type_len = parser->previous.length;
            }

            params[param_count].name           = arena_strndup(parser->arena, p_name.start, p_name.length);
            params[param_count].name_len       = p_name.length;
            params[param_count].type_name      = p_type ? arena_strndup(parser->arena, p_type, p_type_len) : NULL;
            params[param_count].type_len       = p_type_len;
            param_count++;
        } while (match_token(parser, TOK_COMMA));
    }
    consume(parser, TOK_RPAREN, "Expected ')' after parameters");

    const char *ret_type = NULL;
    usize ret_len = 0;
    if (match_token(parser, TOK_ARROW)) {
        advance_parser(parser);
        ret_type = parser->previous.start;
        ret_len  = parser->previous.length;
    }

    consume(parser, TOK_LBRACE, "Expected '{' before function body");
    AstExpr *body = parse_block_expr(parser);

    AstDecl *node = alloc_decl(parser->arena, DECL_FUNCTION, line, col);
    if (node) {
        node->fn_decl.name            = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->fn_decl.name_len        = name_tok.length;
        node->fn_decl.params          = params;
        node->fn_decl.param_count     = param_count;
        node->fn_decl.return_type     = ret_type ? arena_strndup(parser->arena, ret_type, ret_len) : NULL;
        node->fn_decl.return_type_len = ret_len;
        node->fn_decl.body            = body;
        node->fn_decl.is_public       = is_pub;
    }
    return node;
}

static AstDecl *parse_shape_decl(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_IDENT, "Expected shape name");
    Token name_tok = parser->previous;

    consume(parser, TOK_LBRACE, "Expected '{' before shape fields");

    u32 cap = 8;
    u32 field_count = 0;
    AstField *fields = (AstField*)arena_alloc(parser->arena, sizeof(AstField) * cap);

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        if (field_count >= cap) {
            u32 new_cap = cap * 2;
            AstField *new_fields = (AstField*)arena_alloc(parser->arena, sizeof(AstField) * new_cap);
            my_memcpy(new_fields, fields, sizeof(AstField) * cap);
            fields = new_fields;
            cap = new_cap;
        }
        if (check(parser, TOK_IDENT) || check(parser, TOK_CAP) || check(parser, TOK_MUT) || check(parser, TOK_VOLATILE) || check(parser, TOK_REG) || check(parser, TOK_SELF)) {
            advance_parser(parser);
        } else {
            consume(parser, TOK_IDENT, "Expected field name");
        }
        Token f_name = parser->previous;

        const char *f_type = NULL;
        usize f_type_len = 0;
        if (match_token(parser, TOK_COLON)) {
            advance_parser(parser);
            f_type     = parser->previous.start;
            f_type_len = parser->previous.length;
        }

        fields[field_count].name      = arena_strndup(parser->arena, f_name.start, f_name.length);
        fields[field_count].name_len  = f_name.length;
        fields[field_count].type_name = f_type ? arena_strndup(parser->arena, f_type, f_type_len) : NULL;
        fields[field_count].type_len  = f_type_len;
        field_count++;

        if (!match_token(parser, TOK_COMMA) && !check(parser, TOK_RBRACE)) {
            consume(parser, TOK_SEMICOLON, "Expected ',' or ';' between fields");
        }
    }
    consume(parser, TOK_RBRACE, "Expected '}' after shape declaration");

    AstDecl *node = alloc_decl(parser->arena, DECL_SHAPE, line, col);
    if (node) {
        node->shape_decl.name        = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->shape_decl.name_len    = name_tok.length;
        node->shape_decl.fields      = fields;
        node->shape_decl.field_count = field_count;
    }
    return node;
}

static AstDecl *parse_struct_decl(Parser *parser, bool is_packed) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_IDENT, "Expected struct name");
    Token name_tok = parser->previous;

    consume(parser, TOK_LBRACE, "Expected '{' before struct fields");

    u32 cap = 8;
    u32 field_count = 0;
    AstField *fields = (AstField*)arena_alloc(parser->arena, sizeof(AstField) * cap);

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        if (field_count >= cap) {
            u32 new_cap = cap * 2;
            AstField *new_fields = (AstField*)arena_alloc(parser->arena, sizeof(AstField) * new_cap);
            my_memcpy(new_fields, fields, sizeof(AstField) * cap);
            fields = new_fields;
            cap = new_cap;
        }
        if (check(parser, TOK_IDENT) || check(parser, TOK_CAP) || check(parser, TOK_MUT) || check(parser, TOK_VOLATILE) || check(parser, TOK_REG) || check(parser, TOK_SELF)) {
            advance_parser(parser);
        } else {
            consume(parser, TOK_IDENT, "Expected field name");
        }
        Token f_name = parser->previous;

        const char *f_type = NULL;
        usize f_type_len = 0;
        if (match_token(parser, TOK_COLON)) {
            advance_parser(parser);
            f_type     = parser->previous.start;
            f_type_len = parser->previous.length;
        }

        fields[field_count].name      = arena_strndup(parser->arena, f_name.start, f_name.length);
        fields[field_count].name_len  = f_name.length;
        fields[field_count].type_name = f_type ? arena_strndup(parser->arena, f_type, f_type_len) : NULL;
        fields[field_count].type_len  = f_type_len;
        field_count++;

        if (!match_token(parser, TOK_COMMA) && !check(parser, TOK_RBRACE)) {
            consume(parser, TOK_SEMICOLON, "Expected ',' or ';' between fields");
        }
    }
    consume(parser, TOK_RBRACE, "Expected '}' after struct declaration");

    AstDecl *node = alloc_decl(parser->arena, DECL_STRUCT, line, col);
    if (node) {
        node->struct_decl.name        = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->struct_decl.name_len    = name_tok.length;
        node->struct_decl.fields      = fields;
        node->struct_decl.field_count = field_count;
        node->struct_decl.is_packed   = is_packed;
    }
    return node;
}

static AstDecl *parse_reg_block(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_IDENT, "Expected peripheral register block name");
    Token name_tok = parser->previous;

    consume(parser, TOK_AT, "Expected '@' before base address");
    consume(parser, TOK_INT_LIT, "Expected integer/hex base address");
    u64 base_addr = (u64)parser->previous.int_val;

    const char *cap_name = NULL;
    usize cap_len = 0;
    if (match_token(parser, TOK_COLON)) {
        if (match_token(parser, TOK_CAP) || match_token(parser, TOK_CAPABILITY)) {
            consume(parser, TOK_IDENT, "Expected capability token name");
            cap_name = parser->previous.start;
            cap_len  = parser->previous.length;
        }
    }

    consume(parser, TOK_LBRACE, "Expected '{' before register definitions");

    u32 cap = 16;
    u32 reg_count = 0;
    AstRegMember *regs = (AstRegMember*)arena_alloc(parser->arena, sizeof(AstRegMember) * cap);

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        if (reg_count >= cap) {
            u32 new_cap = cap * 2;
            AstRegMember *new_regs = (AstRegMember*)arena_alloc(parser->arena, sizeof(AstRegMember) * new_cap);
            my_memcpy(new_regs, regs, sizeof(AstRegMember) * cap);
            regs = new_regs;
            cap = new_cap;
        }

        bool is_vol = match_token(parser, TOK_VOLATILE);
        consume(parser, TOK_IDENT, "Expected register name");
        Token r_name = parser->previous;

        const char *r_type = "u32";
        usize r_type_len = 3;
        if (match_token(parser, TOK_COLON)) {
            advance_parser(parser);
            r_type     = parser->previous.start;
            r_type_len = parser->previous.length;
        }

        u64 offset = 0;
        if (match_token(parser, TOK_AT)) {
            consume(parser, TOK_INT_LIT, "Expected register offset address");
            offset = (u64)parser->previous.int_val;
        }

        regs[reg_count].name        = arena_strndup(parser->arena, r_name.start, r_name.length);
        regs[reg_count].name_len    = r_name.length;
        regs[reg_count].type_name   = arena_strndup(parser->arena, r_type, r_type_len);
        regs[reg_count].type_len    = r_type_len;
        regs[reg_count].offset      = offset;
        regs[reg_count].is_volatile = is_vol;
        reg_count++;

        if (!match_token(parser, TOK_COMMA) && !check(parser, TOK_RBRACE)) {
            consume(parser, TOK_SEMICOLON, "Expected ',' or ';' between registers");
        }
    }
    consume(parser, TOK_RBRACE, "Expected '}' after register block");

    AstDecl *node = alloc_decl(parser->arena, DECL_REG_BLOCK, line, col);
    if (node) {
        node->reg_block.name         = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->reg_block.name_len     = name_tok.length;
        node->reg_block.base_address = base_addr;
        node->reg_block.cap_token    = cap_name ? arena_strndup(parser->arena, cap_name, cap_len) : NULL;
        node->reg_block.cap_len      = cap_len;
        node->reg_block.registers    = regs;
        node->reg_block.reg_count    = reg_count;
    }
    return node;
}

static AstDecl *parse_isr_decl(Parser *parser) {
    u32 line = parser->previous.line;
    u32 col  = parser->previous.col;

    consume(parser, TOK_LPAREN, "Expected '(' after 'isr'");
    consume(parser, TOK_INT_LIT, "Expected integer interrupt vector number");
    i32 vector_num = (i32)parser->previous.int_val;
    consume(parser, TOK_RPAREN, "Expected ')' after vector number");

    consume(parser, TOK_FN, "Expected 'fn' after isr(N)");
    consume(parser, TOK_IDENT, "Expected ISR handler function name");
    Token name_tok = parser->previous;

    consume(parser, TOK_LPAREN, "Expected '()' for ISR parameter list");
    consume(parser, TOK_RPAREN, "Expected ')' after ISR parameter list");

    consume(parser, TOK_LBRACE, "Expected '{' before ISR body");
    AstExpr *body = parse_block_expr(parser);

    AstDecl *node = alloc_decl(parser->arena, DECL_ISR, line, col);
    if (node) {
        node->isr_decl.name       = arena_strndup(parser->arena, name_tok.start, name_tok.length);
        node->isr_decl.name_len   = name_tok.length;
        node->isr_decl.vector_num = vector_num;
        node->isr_decl.body       = body;
    }
    return node;
}

AstDecl *parser_parse_decl(Parser *parser) {
    bool is_pub = match_token(parser, TOK_PUB);

    if (match_token(parser, TOK_FN)) {
        return parse_fn_decl(parser, is_pub);
    }
    if (match_token(parser, TOK_SHAPE)) {
        return parse_shape_decl(parser);
    }
    if (match_token(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser, FALSE);
    }
    if (match_token(parser, TOK_PACKED)) {
        consume(parser, TOK_STRUCT, "Expected 'struct' after 'packed'");
        return parse_struct_decl(parser, TRUE);
    }
    if (match_token(parser, TOK_REG)) {
        return parse_reg_block(parser);
    }
    if (match_token(parser, TOK_ISR)) {
        return parse_isr_decl(parser);
    }

    /* Top-level statement */
    AstStmt *stmt = parser_parse_stmt(parser);
    if (!stmt) return NULL;

    AstDecl *decl = alloc_decl(parser->arena, DECL_STMT, stmt->line, stmt->col);
    if (decl) decl->stmt = stmt;
    return decl;
}

AstProgram *parser_parse_program(Parser *parser) {
    AstProgram *prog = (AstProgram*)arena_alloc_zero(parser->arena, sizeof(AstProgram));
    if (!prog) return NULL;
    prog->arena = parser->arena;

    u32 cap = 16;
    u32 count = 0;
    AstDecl **decls = (AstDecl**)arena_alloc(parser->arena, sizeof(AstDecl*) * cap);

    while (!check(parser, TOK_EOF)) {
        AstDecl *decl = parser_parse_decl(parser);
        if (decl) {
            if (count >= cap) {
                u32 new_cap = cap * 2;
                AstDecl **new_decls = (AstDecl**)arena_alloc(parser->arena, sizeof(AstDecl*) * new_cap);
                my_memcpy(new_decls, decls, sizeof(AstDecl*) * cap);
                decls = new_decls;
                cap = new_cap;
            }
            decls[count++] = decl;
        } else {
            synchronize(parser);
        }
    }

    prog->decls      = decls;
    prog->decl_count = count;
    return prog;
}
