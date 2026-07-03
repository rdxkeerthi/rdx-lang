/* parser.c — RDX Parser v2 (simplified syntax) */
#include "parser.h"

ASTNode rdx_nodes[MAX_NODES];
int     rdx_node_count = 0;

static Token *cur(Parser *P)            { return &P->tokens[P->pos]; }
static Token *peek(Parser *P, int off)  { int i = P->pos+off; return (i < P->count) ? &P->tokens[i] : &P->tokens[P->count-1]; }
static Token *advance(Parser *P)        { Token *t = cur(P); if (P->pos < P->count-1) P->pos++; return t; }
static bool   check(Parser *P, TokenType t) { return cur(P)->type == t; }
static bool   match(Parser *P, TokenType t) { if (check(P,t)){advance(P);return TRUE;} return FALSE; }
static Token *expect(Parser *P, TokenType t, const char *msg) {
    if (check(P,t)) return advance(P);
    P->had_error = 1;
    rdx_print_err("[RDX PARSE ERROR] line "); rdx_print_num((i64)cur(P)->line);
    rdx_print_err(": "); rdx_print_err(msg);
    rdx_print_err(" (got '"); rdx_print_err(cur(P)->value); rdx_print_err("')\r\n");
    return cur(P);
}
static int new_node(NodeType type, int line) {
    if (rdx_node_count >= (int)MAX_NODES) rdx_panic("parser: MAX_NODES exceeded");
    ASTNode *n = &rdx_nodes[rdx_node_count];
    my_memset(n, 0, sizeof(ASTNode));
    n->type = type; n->line = line;
    n->left = n->right = n->condition = n->body = n->else_body = -1;
    return rdx_node_count++;
}

static int parse_stmt(Parser *P);
static int parse_expr(Parser *P);
static int parse_block(Parser *P);

/* ---- Primary ---- */
static int parse_primary(Parser *P) {
    Token *t = cur(P);
    int line = t->line;

    if (t->type == TOK_NUM_LIT) {
        int n = new_node(NODE_LITERAL_NUM, line);
        const char *s = t->value; i64 v = 0;
        while (*s >= '0' && *s <= '9') v = v*10 + (*s++ - '0');
        rdx_nodes[n].ival = v; advance(P); return n;
    }
    if (t->type == TOK_STR_LIT) {
        int n = new_node(NODE_LITERAL_STR, line);
        my_strcpy(rdx_nodes[n].sval, t->value); advance(P); return n;
    }
    if (t->type == TOK_FLT_LIT) {
        int n = new_node(NODE_LITERAL_FLT, line);
        my_strcpy(rdx_nodes[n].sval, t->value); advance(P); return n;
    }
    /* true / false */
    if (t->type == TOK_TRUE) {
        int n = new_node(NODE_LITERAL_NUM, line); rdx_nodes[n].ival = 1; advance(P); return n;
    }
    if (t->type == TOK_FALSE) {
        int n = new_node(NODE_LITERAL_NUM, line); rdx_nodes[n].ival = 0; advance(P); return n;
    }
    /* [ array ] */
    if (t->type == TOK_LBRACKET) {
        advance(P);
        int n = new_node(NODE_ARRAY, line);
        while (!check(P,TOK_RBRACKET) && !check(P,TOK_EOF)) {
            if (rdx_nodes[n].child_count > 0) expect(P, TOK_COMMA, "expected ','");
            rdx_nodes[n].children[rdx_nodes[n].child_count++] = parse_expr(P);
        }
        expect(P, TOK_RBRACKET, "expected ']'"); return n;
    }
    /* alloc TypeName() */
    if (t->type == TOK_ALLOC) {
        advance(P); int n = new_node(NODE_ALLOC, line);
        Token *tn = expect(P, TOK_IDENT, "expected type after 'alloc'");
        my_strcpy(rdx_nodes[n].rdx_type, tn->value);
        expect(P, TOK_LPAREN, "expected '('"); expect(P, TOK_RPAREN, "expected ')'");
        return n;
    }
    /* call("..." [args]) */
    if (t->type == TOK_CALL) {
        advance(P); int n = new_node(NODE_SYS_CALL, line);
        expect(P, TOK_LPAREN, "expected '(' after 'call'");
        if (check(P, TOK_STR_LIT)) {
            Token *arg = advance(P); my_strcpy(rdx_nodes[n].sval, arg->value);
            if (check(P, TOK_LBRACKET)) rdx_nodes[n].left = parse_primary(P);
            else rdx_nodes[n].left = -1;
        } else {
            rdx_nodes[n].sval[0] = '\0';
            int arr = new_node(NODE_ARRAY, line);
            rdx_nodes[arr].children[0] = parse_expr(P);
            rdx_nodes[arr].child_count = 1;
            rdx_nodes[n].left = arr;
        }
        expect(P, TOK_RPAREN, "expected ')'"); return n;
    }
    /* get("prompt") — read a line from stdin */
    if (t->type == TOK_GET) {
        advance(P);
        int n = new_node(NODE_GET, line);
        rdx_nodes[n].sval[0] = '\0';   /* default: no prompt */
        expect(P, TOK_LPAREN, "expected '(' after 'get'");
        if (check(P, TOK_STR_LIT)) {
            Token *prompt = advance(P);
            my_strcpy(rdx_nodes[n].sval, prompt->value);  /* store prompt */
        }
        expect(P, TOK_RPAREN, "expected ')'");
        return n;
    }
    /* identifier, type-kw, fn call, member access */
    if (t->type == TOK_IDENT || tok_is_type(t->type)) {
        advance(P);
        if (check(P, TOK_LPAREN)) {
            advance(P); int n = new_node(NODE_FN_CALL, line);
            my_strcpy(rdx_nodes[n].sval, t->value);
            while (!check(P,TOK_RPAREN) && !check(P,TOK_EOF)) {
                if (rdx_nodes[n].child_count > 0) expect(P, TOK_COMMA, "expected ','");
                rdx_nodes[n].children[rdx_nodes[n].child_count++] = parse_expr(P);
            }
            expect(P, TOK_RPAREN, "expected ')'"); return n;
        }
        if (check(P, TOK_DOT)) {
            advance(P);
            Token *field = expect(P, TOK_IDENT, "expected field name");
            int n = new_node(NODE_MEMBER_ACCESS, line);
            my_strcpy(rdx_nodes[n].sval, t->value);
            my_strcpy(rdx_nodes[n].rdx_type, field->value);
            return n;
        }
        int n = new_node(NODE_IDENT, line);
        my_strcpy(rdx_nodes[n].sval, t->value); return n;
    }
    /* (expr) */
    if (t->type == TOK_LPAREN) {
        advance(P); int inner = parse_expr(P);
        expect(P, TOK_RPAREN, "expected ')'"); return inner;
    }

    P->had_error = 1;
    rdx_print_err("[RDX PARSE ERROR] line "); rdx_print_num((i64)line);
    rdx_print_err(": unexpected token '"); rdx_print_err(t->value); rdx_print_err("'\r\n");
    advance(P); return new_node(NODE_LITERAL_NUM, line);
}

/* not expr */
static int parse_unary(Parser *P) {
    if (check(P, TOK_NOT) || check(P, TOK_MINUS)) {
        Token *op = advance(P);
        int n = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->type == TOK_NOT ? "not" : "neg");
        rdx_nodes[n].left = parse_unary(P);
        rdx_nodes[n].right = -1; return n;
    }
    return parse_primary(P);
}
static int parse_mul(Parser *P) {
    int left = parse_unary(P);
    while (check(P,TOK_STAR)||check(P,TOK_SLASH)||check(P,TOK_PERCENT)) {
        Token *op = advance(P); int right = parse_unary(P);
        int n = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left = left; rdx_nodes[n].right = right; left = n;
    }
    return left;
}
static int parse_add(Parser *P) {
    int left = parse_mul(P);
    while (check(P,TOK_PLUS)||check(P,TOK_MINUS)) {
        Token *op = advance(P); int right = parse_mul(P);
        int n = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left = left; rdx_nodes[n].right = right; left = n;
    }
    return left;
}
static int parse_cmp(Parser *P) {
    int left = parse_add(P);
    while (check(P,TOK_EQ)||check(P,TOK_NEQ)||check(P,TOK_LT)||
           check(P,TOK_GT)||check(P,TOK_LTE)||check(P,TOK_GTE)) {
        Token *op = advance(P); int right = parse_add(P);
        int n = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left = left; rdx_nodes[n].right = right; left = n;
    }
    return left;
}
static int parse_expr(Parser *P) {
    int left = parse_cmp(P);
    while (check(P,TOK_AND)||check(P,TOK_OR)) {
        Token *op = advance(P); int right = parse_cmp(P);
        int n = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left = left; rdx_nodes[n].right = right; left = n;
    }
    return left;
}

/* ---- Block ---- */
static int parse_block(Parser *P) {
    int line = cur(P)->line;
    expect(P, TOK_LBRACE, "expected '{'");
    int n = new_node(NODE_BLOCK, line);
    while (!check(P,TOK_RBRACE) && !check(P,TOK_EOF)) {
        int s = parse_stmt(P);
        if (s >= 0) rdx_nodes[n].children[rdx_nodes[n].child_count++] = s;
    }
    expect(P, TOK_RBRACE, "expected '}'"); return n;
}

/* ---- Statement ---- */
static int parse_stmt(Parser *P) {
    int line = cur(P)->line;

    /* ret expr */
    if (check(P,TOK_RET)) {
        advance(P); int n = new_node(NODE_RETURN, line);
        rdx_nodes[n].left = parse_expr(P);
        match(P, TOK_SEMICOLON); return n;
    }

    /* drop ident */
    if (check(P,TOK_DROP)) {
        advance(P); int n = new_node(NODE_DROP, line);
        Token *id = expect(P, TOK_IDENT, "expected name after 'drop'");
        my_strcpy(rdx_nodes[n].sval, id->value);
        match(P, TOK_SEMICOLON); return n;
    }

    /* break */
    if (check(P,TOK_BREAK)) {
        advance(P); int n = new_node(NODE_RETURN, line); /* reuse RETURN for break */
        rdx_nodes[n].ival = -999; /* sentinel: break */
        match(P, TOK_SEMICOLON); return n;
    }

    /* continue — skip rest of loop body, go to next iteration */
    if (check(P,TOK_CONTINUE)) {
        advance(P); int n = new_node(NODE_CONTINUE, line);
        match(P, TOK_SEMICOLON); return n;
    }

    /* safe { body } fail errname { handler }
       — structured error handling block */
    if (check(P,TOK_SAFE)) {
        advance(P);
        int n = new_node(NODE_SAFE_FAIL, line);
        rdx_nodes[n].body = parse_block(P);  /* safe body */
        if (check(P,TOK_FAIL)) {
            advance(P);
            /* optional error variable name */
            if (check(P,TOK_IDENT)) {
                Token *errvar = advance(P);
                my_strcpy(rdx_nodes[n].sval, errvar->value);
            }
            rdx_nodes[n].else_body = parse_block(P); /* fail body */
        }
        return n;
    }

    /* match val { val -> { } ... _ -> { } }
       — pattern matching / switch */
    if (check(P,TOK_MATCH)) {
        advance(P);
        int n = new_node(NODE_MATCH, line);
        rdx_nodes[n].condition = parse_expr(P); /* value to match */
        expect(P, TOK_LBRACE, "expected '{' after match value");
        /* Each arm: val -> { stmts }  or  _ -> { stmts } */
        while (!check(P,TOK_RBRACE) && !check(P,TOK_EOF)) {
            if (rdx_nodes[n].child_count + 2 >= 60) break;
            /* parse the pattern (literal or _) */
            int pat_node;
            if (check(P,TOK_IDENT) && my_strcmp(cur(P)->value, "_") == 0) {
                /* wildcard default arm */
                pat_node = new_node(NODE_LITERAL_STR, cur(P)->line);
                my_strcpy(rdx_nodes[pat_node].sval, "_");
                advance(P);
            } else {
                pat_node = parse_expr(P);
            }
            expect(P, TOK_ARROW, "expected '->' in match arm");
            int body_node = parse_block(P);
            rdx_nodes[n].children[rdx_nodes[n].child_count++] = pat_node;
            rdx_nodes[n].children[rdx_nodes[n].child_count++] = body_node;
        }
        expect(P, TOK_RBRACE, "expected '}' to close match");
        return n;
    }

    /* fix NAME = value  (constant) */
    if (check(P,TOK_FIX)) {
        advance(P); int n = new_node(NODE_VAR_DECL, line);
        Token *name = expect(P, TOK_IDENT, "expected constant name");
        my_strcpy(rdx_nodes[n].sval, name->value);
        my_strcpy(rdx_nodes[n].rdx_type, "fix");
        expect(P, TOK_ASSIGN, "expected '=' after constant name");
        rdx_nodes[n].left = parse_expr(P);
        match(P, TOK_SEMICOLON); return n;
    }

    /* load module */
    if (check(P,TOK_LOAD)) {
        advance(P); int n = new_node(NODE_IDENT, line); /* simple marker */
        Token *mod = expect(P, TOK_IDENT, "expected module name");
        my_strcpy(rdx_nodes[n].sval, mod->value);
        my_strcpy(rdx_nodes[n].rdx_type, "load");
        match(P, TOK_SEMICOLON); return n;
    }

    /*
     * if cond { } elif cond { } else { }
     * NO parentheses required — condition ends at '{'
     */
    if (check(P,TOK_IF)) {
        advance(P); int n = new_node(NODE_IF, line);
        rdx_nodes[n].condition = parse_expr(P);
        rdx_nodes[n].body = parse_block(P);
        if (check(P,TOK_ELIF)) {
            /* treat elif as nested if in else_body */
            int block_n = new_node(NODE_BLOCK, cur(P)->line);
            int elif_stmt = parse_stmt(P);  /* recurse: elif handled as if */
            rdx_nodes[block_n].children[rdx_nodes[block_n].child_count++] = elif_stmt;
            rdx_nodes[n].else_body = block_n;
        } else if (check(P,TOK_ELSE)) {
            advance(P); rdx_nodes[n].else_body = parse_block(P);
        }
        return n;
    }
    /* elif — same as if but triggered from above */
    if (check(P,TOK_ELIF)) {
        advance(P); int n = new_node(NODE_IF, line);
        rdx_nodes[n].condition = parse_expr(P);
        rdx_nodes[n].body = parse_block(P);
        if (check(P,TOK_ELIF)) {
            int block_n = new_node(NODE_BLOCK, cur(P)->line);
            rdx_nodes[block_n].children[rdx_nodes[block_n].child_count++] = parse_stmt(P);
            rdx_nodes[n].else_body = block_n;
        } else if (check(P,TOK_ELSE)) {
            advance(P); rdx_nodes[n].else_body = parse_block(P);
        }
        return n;
    }

    /*
     * loop Forms:
     *   loop { }                 — infinite
     *   loop cond { }            — while-style
     *   loop 0..10 as i { }      — range loop
     *   loop items as item { }   — iterator loop
     */
    if (check(P,TOK_LOOP)) {
        advance(P); int n = new_node(NODE_LOOP, line);
        if (check(P,TOK_LBRACE)) {
            /* infinite loop — condition node = -1 (already) */
            rdx_nodes[n].body = parse_block(P);
        } else {
            /* parse the "start..end as var" or "expr as var" or plain "cond" */
            int start_expr = parse_expr(P);
            if (check(P,TOK_DOTDOT)) {
                /* range: start..end as i */
                advance(P);
                int end_expr = parse_expr(P);
                /* store range as ARRAY node: [start, end] */
                int range = new_node(NODE_ARRAY, line);
                rdx_nodes[range].children[0] = start_expr;
                rdx_nodes[range].children[1] = end_expr;
                rdx_nodes[range].child_count  = 2;
                rdx_nodes[n].condition = range;
                my_strcpy(rdx_nodes[n].rdx_type, "range");
                if (check(P,TOK_AS)) {
                    advance(P);
                    Token *var = expect(P, TOK_IDENT, "expected loop variable after 'as'");
                    my_strcpy(rdx_nodes[n].sval, var->value);
                }
            } else if (check(P,TOK_AS)) {
                /* iterator: items as item */
                rdx_nodes[n].condition = start_expr;
                advance(P);
                Token *var = expect(P, TOK_IDENT, "expected loop variable after 'as'");
                my_strcpy(rdx_nodes[n].sval, var->value);
                my_strcpy(rdx_nodes[n].rdx_type, "iter");
            } else {
                /* while-style: loop cond { } */
                rdx_nodes[n].condition = start_expr;
            }
            rdx_nodes[n].body = parse_block(P);
        }
        return n;
    }

    /* when val { pattern -> { } ... } */
    if (check(P,TOK_WHEN)) {
        advance(P); int n = new_node(NODE_IF, line); /* reuse IF chain */
        rdx_nodes[n].condition = parse_expr(P);
        rdx_nodes[n].body = parse_block(P);
        my_strcpy(rdx_nodes[n].rdx_type, "when");
        return n;
    }

    /* sys.rdx { } */
    if (check(P,TOK_SYS)) {
        advance(P);
        expect(P, TOK_DOT,   "expected '.' after 'sys'");
        expect(P, TOK_IDENT, "expected 'rdx' after 'sys.'");
        int n = new_node(NODE_SYS_BLOCK, line);
        rdx_nodes[n].body = parse_block(P); return n;
    }

    /* call(...) — standalone print */
    if (check(P,TOK_CALL)) {
        int n = parse_primary(P); match(P, TOK_SEMICOLON); return n;
    }

    /*
     * mut? TYPE name = value
     *   e.g.  num x = 10
     *          mut num count = 0
     *          chr msg = "hello"
     *          num = 25        (shorthand: type IS the name)
     */
    bool is_mut = FALSE;
    if (check(P,TOK_MUT)) { advance(P); is_mut = TRUE; }
    if (tok_is_type(cur(P)->type)) {
        Token *type_tok = advance(P);
        int n = new_node(NODE_VAR_DECL, line);
        my_strcpy(rdx_nodes[n].rdx_type, type_tok->value);
        if (is_mut) rdx_nodes[n].ival = 1; /* ival=1 = mutable flag */

        if (check(P,TOK_IDENT)) {
            /* named variable: num count = 42 */
            Token *name_tok = advance(P);
            my_strcpy(rdx_nodes[n].sval, name_tok->value);
        } else {
            /* shorthand: num = 25  — type keyword IS the name */
            my_strcpy(rdx_nodes[n].sval, type_tok->value);
        }
        expect(P, TOK_ASSIGN, "expected '=' after variable name");
        rdx_nodes[n].left = parse_expr(P);
        match(P, TOK_SEMICOLON); return n;
    }
    if (is_mut) {
        /* mut without type — error, but recover */
        P->had_error = 1;
        rdx_print_err("[RDX PARSE ERROR]: 'mut' must be followed by a type keyword\r\n");
    }

    /* Blueprint/shape-typed variable:  ShapeName varName = value */
    if (check(P,TOK_IDENT) && peek(P,1)->type == TOK_IDENT) {
        Token *type_tok = advance(P);
        int n = new_node(NODE_VAR_DECL, line);
        my_strcpy(rdx_nodes[n].rdx_type, type_tok->value);
        Token *id = advance(P); my_strcpy(rdx_nodes[n].sval, id->value);
        expect(P, TOK_ASSIGN, "expected '='");
        rdx_nodes[n].left = parse_expr(P);
        match(P, TOK_SEMICOLON); return n;
    }

    /* ident = expr  or  ident.field = expr */
    if (check(P,TOK_IDENT)) {
        Token *id = advance(P);
        if (check(P,TOK_DOT)) {
            advance(P);
            Token *field = expect(P, TOK_IDENT, "expected field name");
            expect(P, TOK_ASSIGN, "expected '='");
            int n = new_node(NODE_ASSIGN, line);
            my_strcpy(rdx_nodes[n].sval, id->value);
            my_strcpy(rdx_nodes[n].rdx_type, field->value);
            rdx_nodes[n].left = parse_expr(P);
            match(P, TOK_SEMICOLON); return n;
        }
        if (check(P,TOK_ASSIGN)) {
            advance(P); int n = new_node(NODE_ASSIGN, line);
            my_strcpy(rdx_nodes[n].sval, id->value);
            rdx_nodes[n].left = parse_expr(P);
            match(P, TOK_SEMICOLON); return n;
        }
        if (check(P,TOK_LPAREN)) {
            P->pos--;
            int n = parse_primary(P);
            match(P, TOK_SEMICOLON); return n;
        }
    }

    /* skip unknown */
    rdx_print_err("[RDX PARSE ERROR] line "); rdx_print_num((i64)line);
    rdx_print_err(": unexpected token '"); rdx_print_err(cur(P)->value); rdx_print_err("'\r\n");
    P->had_error = 1; advance(P); return -1;
}

/* ---- make funcName(type param, ...) -> type { } ---- */
static int parse_fn_decl(Parser *P) {
    int line = cur(P)->line;
    advance(P); /* consume 'make' */
    int n = new_node(NODE_FN_DECL, line);
    Token *name = expect(P, TOK_IDENT, "expected function name after 'make'");
    my_strcpy(rdx_nodes[n].sval, name->value);
    expect(P, TOK_LPAREN, "expected '('");
    while (!check(P,TOK_RPAREN) && !check(P,TOK_EOF)) {
        if (rdx_nodes[n].child_count > 0) expect(P, TOK_COMMA, "expected ','");
        int pn = new_node(NODE_VAR_DECL, cur(P)->line);
        /* new style: type name   e.g.  num a */
        Token *ptype = advance(P);
        my_strcpy(rdx_nodes[pn].rdx_type, ptype->value);
        Token *pname = expect(P, TOK_IDENT, "expected parameter name");
        my_strcpy(rdx_nodes[pn].sval, pname->value);
        rdx_nodes[n].children[rdx_nodes[n].child_count++] = pn;
    }
    expect(P, TOK_RPAREN, "expected ')'");
    if (check(P,TOK_ARROW)) {
        advance(P); Token *ret = advance(P);
        my_strcpy(rdx_nodes[n].rdx_type, ret->value);
    }
    rdx_nodes[n].body = parse_block(P); return n;
}

/* ---- shape ShapeName { type field \n ... } ---- */
static int parse_shape_decl(Parser *P) {
    int line = cur(P)->line;
    advance(P); /* consume 'shape' */
    int n = new_node(NODE_BLUEPRINT, line);
    Token *name = expect(P, TOK_IDENT, "expected shape name");
    my_strcpy(rdx_nodes[n].sval, name->value);
    expect(P, TOK_LBRACE, "expected '{'");
    while (!check(P,TOK_RBRACE) && !check(P,TOK_EOF)) {
        Token *ftype = advance(P);
        Token *fname = expect(P, TOK_IDENT, "expected field name");
        match(P, TOK_SEMICOLON); /* optional semicolon between fields */
        int fn = new_node(NODE_VAR_DECL, fname->line);
        my_strcpy(rdx_nodes[fn].sval, fname->value);
        my_strcpy(rdx_nodes[fn].rdx_type, ftype->value);
        rdx_nodes[n].children[rdx_nodes[n].child_count++] = fn;
    }
    expect(P, TOK_RBRACE, "expected '}'"); return n;
}

/* ---- Top-level ---- */
static int parse_program(Parser *P) {
    int root = new_node(NODE_PROGRAM, 0);
    while (!check(P,TOK_EOF)) {
        int n = -1;
        if (check(P,TOK_MAKE))  n = parse_fn_decl(P);
        else if (check(P,TOK_SHAPE)) n = parse_shape_decl(P);
        else                    n = parse_stmt(P);
        if (n >= 0) rdx_nodes[root].children[rdx_nodes[root].child_count++] = n;
    }
    return root;
}

void parser_init(Parser *P, Token *tokens, int count) {
    P->tokens = tokens; P->count = count; P->pos = 0; P->had_error = 0;
}
int parser_parse(Parser *P) { rdx_node_count = 0; return parse_program(P); }

static const char *node_name(NodeType t) {
    static const char *names[] = {
        "PROGRAM","VAR_DECL","FN_DECL","RETURN","IF","LOOP","BLUEPRINT",
        "ALLOC","DROP","ASSIGN","FN_CALL","SYS_CALL","BINARY_OP",
        "LIT_NUM","LIT_STR","LIT_FLT","IDENT","SYS_BLOCK","MEMBER_ACCESS","BLOCK","ARRAY"
    };
    return (t >= 0 && t < 21) ? names[t] : "???";
}
void parser_dump(int idx, int depth) {
    if (idx < 0 || idx >= rdx_node_count) return;
    ASTNode *n = &rdx_nodes[idx];
    for (int i = 0; i < depth*2; i++) rdx_print_char(' ');
    rdx_print(node_name(n->type));
    if (n->sval[0])     { rdx_print(" \""); rdx_print(n->sval); rdx_print("\""); }
    if (n->ival)        { rdx_print(" ival="); rdx_print_num(n->ival); }
    if (n->op[0])       { rdx_print(" op="); rdx_print(n->op); }
    if (n->rdx_type[0]) { rdx_print(" type="); rdx_print(n->rdx_type); }
    rdx_print("\r\n");
    if (n->condition >= 0) parser_dump(n->condition, depth+1);
    if (n->left      >= 0) parser_dump(n->left,      depth+1);
    if (n->right     >= 0) parser_dump(n->right,     depth+1);
    if (n->body      >= 0) parser_dump(n->body,      depth+1);
    if (n->else_body >= 0) parser_dump(n->else_body, depth+1);
    for (int i = 0; i < n->child_count; i++) parser_dump(n->children[i], depth+1);
}
