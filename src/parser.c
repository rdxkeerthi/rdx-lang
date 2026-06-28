/* parser.c — RDX Recursive-Descent Parser
   Deps: parser.h (which includes lexer.h, core.h) */
#include "parser.h"

/* ---- Global node pool ---------------------------------------- */
ASTNode rdx_nodes[MAX_NODES];
int     rdx_node_count = 0;

/* ---- Internal helpers ---------------------------------------- */

static Token *cur(Parser *P)  { return &P->tokens[P->pos]; }
static Token *peek(Parser *P, int off) {
    int i = P->pos + off;
    return (i < P->count) ? &P->tokens[i] : &P->tokens[P->count - 1];
}

static Token *advance(Parser *P) {
    Token *t = cur(P);
    if (P->pos < P->count - 1) P->pos++;
    return t;
}

static bool check(Parser *P, TokenType t) { return cur(P)->type == t; }

static bool match(Parser *P, TokenType t) {
    if (check(P, t)) { advance(P); return TRUE; }
    return FALSE;
}

static Token *expect(Parser *P, TokenType t, const char *msg) {
    if (check(P, t)) return advance(P);
    P->had_error = 1;
    rdx_print_err("[RDX PARSE ERROR] line ");
    rdx_print_num((i64)cur(P)->line);
    rdx_print_err(": "); rdx_print_err(msg);
    rdx_print_err(" (got '"); rdx_print_err(cur(P)->value);
    rdx_print_err("')\r\n");
    return cur(P);
}

static int new_node(NodeType type, int line) {
    if (rdx_node_count >= (int)MAX_NODES) {
        rdx_panic("parser: MAX_NODES exceeded");
    }
    ASTNode *n = &rdx_nodes[rdx_node_count];
    my_memset(n, 0, sizeof(ASTNode));
    n->type      = type;
    n->line      = line;
    n->left      = -1;
    n->right     = -1;
    n->condition = -1;
    n->body      = -1;
    n->else_body = -1;
    return rdx_node_count++;
}

/* Forward declarations */
static int parse_stmt(Parser *P);
static int parse_expr(Parser *P);
static int parse_block(Parser *P);

/* ---- Primary expression -------------------------------------- */
/* Handles: literals, identifiers, member access, alloc, call, (expr) */
static int parse_primary(Parser *P) {
    Token *t = cur(P);
    int line  = t->line;

    /* Numeric literal */
    if (t->type == TOK_NUM_LIT) {
        int n = new_node(NODE_LITERAL_NUM, line);
        rdx_nodes[n].ival = 0;
        /* Manual atoi */
        const char *s = t->value;
        i64 v = 0;
        while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
        rdx_nodes[n].ival = v;
        advance(P);
        return n;
    }

    /* String literal */
    if (t->type == TOK_STR_LIT) {
        int n = new_node(NODE_LITERAL_STR, line);
        my_strcpy(rdx_nodes[n].sval, t->value);
        advance(P);
        return n;
    }

    /* alloc TypeName() */
    if (t->type == TOK_ALLOC) {
        advance(P);
        int n = new_node(NODE_ALLOC, line);
        Token *tname = expect(P, TOK_IDENT, "expected type name after 'alloc'");
        my_strcpy(rdx_nodes[n].rdx_type, tname->value);
        expect(P, TOK_LPAREN, "expected '(' after type name in alloc");
        expect(P, TOK_RPAREN, "expected ')' after alloc args");
        return n;
    }

    /* call("string") — sys call within sys.rdx block */
    if (t->type == TOK_CALL) {
        advance(P);
        int n = new_node(NODE_SYS_CALL, line);
        expect(P, TOK_LPAREN, "expected '(' after 'call'");
        Token *arg = expect(P, TOK_STR_LIT, "expected string literal in call()");
        my_strcpy(rdx_nodes[n].sval, arg->value);
        expect(P, TOK_RPAREN, "expected ')' to close call()");
        return n;
    }

    /* Identifier or function call or member access */
    if (t->type == TOK_IDENT) {
        advance(P);
        /* Function call: ident( ... ) */
        if (check(P, TOK_LPAREN)) {
            advance(P);
            int n = new_node(NODE_FN_CALL, line);
            my_strcpy(rdx_nodes[n].sval, t->value);
            while (!check(P, TOK_RPAREN) && !check(P, TOK_EOF)) {
                if (n > 0 && rdx_nodes[n].child_count > 0)
                    expect(P, TOK_COMMA, "expected ',' between arguments");
                int arg = parse_expr(P);
                rdx_nodes[n].children[rdx_nodes[n].child_count++] = arg;
            }
            expect(P, TOK_RPAREN, "expected ')' to close function call");
            return n;
        }
        /* Member access: ident.field */
        if (check(P, TOK_DOT)) {
            advance(P);
            Token *field = expect(P, TOK_IDENT, "expected field name after '.'");
            int n = new_node(NODE_MEMBER_ACCESS, line);
            my_strcpy(rdx_nodes[n].sval, t->value);   /* object name */
            my_strcpy(rdx_nodes[n].rdx_type, field->value); /* field name */
            return n;
        }
        /* Plain identifier */
        int n = new_node(NODE_IDENT, line);
        my_strcpy(rdx_nodes[n].sval, t->value);
        return n;
    }

    /* Parenthesised expression */
    if (t->type == TOK_LPAREN) {
        advance(P);
        int inner = parse_expr(P);
        expect(P, TOK_RPAREN, "expected ')'");
        return inner;
    }

    P->had_error = 1;
    rdx_print_err("[RDX PARSE ERROR] line ");
    rdx_print_num((i64)line);
    rdx_print_err(": unexpected token '");
    rdx_print_err(t->value);
    rdx_print_err("' in expression\r\n");
    advance(P);
    return new_node(NODE_LITERAL_NUM, line);
}

/* ---- Multiplicative: * / ------------------------------------ */
static int parse_multiplicative(Parser *P) {
    int left = parse_primary(P);
    while (check(P, TOK_STAR) || check(P, TOK_SLASH)) {
        Token *op = advance(P);
        int right  = parse_primary(P);
        int n      = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left  = left;
        rdx_nodes[n].right = right;
        left = n;
    }
    return left;
}

/* ---- Additive: + - ------------------------------------------ */
static int parse_additive(Parser *P) {
    int left = parse_multiplicative(P);
    while (check(P, TOK_PLUS) || check(P, TOK_MINUS)) {
        Token *op = advance(P);
        int right  = parse_multiplicative(P);
        int n      = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left  = left;
        rdx_nodes[n].right = right;
        left = n;
    }
    return left;
}

/* ---- Comparison: == != < > <= >= ---------------------------- */
static int parse_expr(Parser *P) {
    int left = parse_additive(P);
    while (check(P, TOK_EQ)  || check(P, TOK_NEQ) ||
           check(P, TOK_LT)  || check(P, TOK_GT)  ||
           check(P, TOK_LTE) || check(P, TOK_GTE)) {
        Token *op = advance(P);
        int right  = parse_additive(P);
        int n      = new_node(NODE_BINARY_OP, op->line);
        my_strcpy(rdx_nodes[n].op, op->value);
        rdx_nodes[n].left  = left;
        rdx_nodes[n].right = right;
        left = n;
    }
    return left;
}

/* ---- Block { stmt; stmt; } ---------------------------------- */
static int parse_block(Parser *P) {
    int line = cur(P)->line;
    expect(P, TOK_LBRACE, "expected '{'");
    int n = new_node(NODE_BLOCK, line);
    while (!check(P, TOK_RBRACE) && !check(P, TOK_EOF)) {
        int s = parse_stmt(P);
        if (s >= 0) rdx_nodes[n].children[rdx_nodes[n].child_count++] = s;
    }
    expect(P, TOK_RBRACE, "expected '}'");
    return n;
}

/* ---- Statement --------------------------------------------- */
static int parse_stmt(Parser *P) {
    int line = cur(P)->line;

    /* return expr; */
    if (check(P, TOK_RETURN)) {
        advance(P);
        int n = new_node(NODE_RETURN, line);
        rdx_nodes[n].left = parse_expr(P);
        expect(P, TOK_SEMICOLON, "expected ';' after return");
        return n;
    }

    /* drop ident; */
    if (check(P, TOK_DROP)) {
        advance(P);
        int n = new_node(NODE_DROP, line);
        Token *id = expect(P, TOK_IDENT, "expected identifier after 'drop'");
        my_strcpy(rdx_nodes[n].sval, id->value);
        expect(P, TOK_SEMICOLON, "expected ';' after drop");
        return n;
    }

    /* if (cond) { } else { } */
    if (check(P, TOK_IF)) {
        advance(P);
        int n = new_node(NODE_IF, line);
        expect(P, TOK_LPAREN, "expected '(' after 'if'");
        rdx_nodes[n].condition = parse_expr(P);
        expect(P, TOK_RPAREN, "expected ')' after if condition");
        rdx_nodes[n].body = parse_block(P);
        if (check(P, TOK_ELSE)) {
            advance(P);
            if (check(P, TOK_IF)) {
                int block_n = new_node(NODE_BLOCK, cur(P)->line);
                int if_stmt = parse_stmt(P);
                rdx_nodes[block_n].children[rdx_nodes[block_n].child_count++] = if_stmt;
                rdx_nodes[n].else_body = block_n;
            } else {
                rdx_nodes[n].else_body = parse_block(P);
            }
        }
        return n;
    }

    /* loop (cond) { } */
    if (check(P, TOK_LOOP)) {
        advance(P);
        int n = new_node(NODE_LOOP, line);
        expect(P, TOK_LPAREN, "expected '(' after 'loop'");
        rdx_nodes[n].condition = parse_expr(P);
        expect(P, TOK_RPAREN, "expected ')' after loop condition");
        rdx_nodes[n].body = parse_block(P);
        return n;
    }

    /* sys.rdx { call("..."); } */
    if (check(P, TOK_SYS)) {
        advance(P);
        expect(P, TOK_DOT,   "expected '.' after 'sys'");
        expect(P, TOK_IDENT, "expected 'rdx' after 'sys.'");
        int n = new_node(NODE_SYS_BLOCK, line);
        rdx_nodes[n].body = parse_block(P);
        return n;
    }

    /* call("..."); — standalone sys call */
    if (check(P, TOK_CALL)) {
        int n = parse_primary(P);
        expect(P, TOK_SEMICOLON, "expected ';' after call()");
        return n;
    }

    /* Type variable declaration:  num/str ident : expr;
       OR blueprint type:          TypeName ident : alloc TypeName(); */
    if (check(P, TOK_NUM) || check(P, TOK_STR)) {
        Token *type_tok = advance(P);
        int n = new_node(NODE_VAR_DECL, line);
        my_strcpy(rdx_nodes[n].rdx_type, type_tok->value);
        Token *id = expect(P, TOK_IDENT, "expected variable name");
        my_strcpy(rdx_nodes[n].sval, id->value);
        expect(P, TOK_ASSIGN, "expected ':' after variable name");
        rdx_nodes[n].left = parse_expr(P);
        expect(P, TOK_SEMICOLON, "expected ';' after variable declaration");
        return n;
    }

    /* Blueprint-typed variable:  Player p : alloc Player(); */
    if (check(P, TOK_IDENT) && peek(P, 1)->type == TOK_IDENT) {
        Token *type_tok = advance(P);
        int n = new_node(NODE_VAR_DECL, line);
        my_strcpy(rdx_nodes[n].rdx_type, type_tok->value);
        Token *id = advance(P);
        my_strcpy(rdx_nodes[n].sval, id->value);
        expect(P, TOK_ASSIGN, "expected ':' after variable name");
        rdx_nodes[n].left = parse_expr(P);
        expect(P, TOK_SEMICOLON, "expected ';'");
        return n;
    }

    /* Assignment or member assignment:  ident : expr;  /  ident.field : expr; */
    if (check(P, TOK_IDENT)) {
        Token *id = advance(P);
        /* member assignment: p.health : 100; */
        if (check(P, TOK_DOT)) {
            advance(P);
            Token *field = expect(P, TOK_IDENT, "expected field name");
            expect(P, TOK_ASSIGN, "expected ':' after field name");
            int n = new_node(NODE_ASSIGN, line);
            my_strcpy(rdx_nodes[n].sval, id->value);
            my_strcpy(rdx_nodes[n].rdx_type, field->value);
            rdx_nodes[n].left = parse_expr(P);
            expect(P, TOK_SEMICOLON, "expected ';'");
            return n;
        }
        /* plain assignment: ident : expr; */
        if (check(P, TOK_ASSIGN)) {
            advance(P);
            int n = new_node(NODE_ASSIGN, line);
            my_strcpy(rdx_nodes[n].sval, id->value);
            rdx_nodes[n].left = parse_expr(P);
            expect(P, TOK_SEMICOLON, "expected ';'");
            return n;
        }
        /* Bare function call statement: foo(args); */
        if (check(P, TOK_LPAREN)) {
            /* back up and let parse_primary handle it */
            P->pos--;
            int n = parse_primary(P);
            expect(P, TOK_SEMICOLON, "expected ';' after function call");
            return n;
        }
    }

    /* Skip unknown token to avoid infinite loop */
    rdx_print_err("[RDX PARSE ERROR] line ");
    rdx_print_num((i64)line);
    rdx_print_err(": unexpected token '");
    rdx_print_err(cur(P)->value);
    rdx_print_err("'\r\n");
    P->had_error = 1;
    advance(P);
    return -1;
}

/* ---- fn declaration ----------------------------------------- */
static int parse_fn_decl(Parser *P) {
    int line = cur(P)->line;
    advance(P); /* consume 'fn' */
    int n = new_node(NODE_FN_DECL, line);
    Token *name = expect(P, TOK_IDENT, "expected function name");
    my_strcpy(rdx_nodes[n].sval, name->value);

    /* Parameter list: (param: type, ...) */
    expect(P, TOK_LPAREN, "expected '(' after function name");
    while (!check(P, TOK_RPAREN) && !check(P, TOK_EOF)) {
        if (rdx_nodes[n].child_count > 0)
            expect(P, TOK_COMMA, "expected ',' between parameters");
        /* param node: sval=name, rdx_type=type */
        int pn = new_node(NODE_VAR_DECL, cur(P)->line);
        Token *pname = expect(P, TOK_IDENT, "expected parameter name");
        my_strcpy(rdx_nodes[pn].sval, pname->value);
        expect(P, TOK_ASSIGN, "expected ':' after parameter name");
        Token *ptype = advance(P); /* num / str / TypeName */
        my_strcpy(rdx_nodes[pn].rdx_type, ptype->value);
        rdx_nodes[n].children[rdx_nodes[n].child_count++] = pn;
    }
    expect(P, TOK_RPAREN, "expected ')'");

    /* Return type: -> type */
    if (check(P, TOK_ARROW)) {
        advance(P);
        Token *ret = advance(P);
        my_strcpy(rdx_nodes[n].rdx_type, ret->value);
    }

    rdx_nodes[n].body = parse_block(P);
    return n;
}

/* ---- blueprint declaration ---------------------------------- */
static int parse_blueprint_decl(Parser *P) {
    int line = cur(P)->line;
    advance(P); /* consume 'blueprint' */
    int n = new_node(NODE_BLUEPRINT, line);
    Token *name = expect(P, TOK_IDENT, "expected blueprint name");
    my_strcpy(rdx_nodes[n].sval, name->value);
    expect(P, TOK_LBRACE, "expected '{'");
    while (!check(P, TOK_RBRACE) && !check(P, TOK_EOF)) {
        /* field: type name; */
        Token *ftype = advance(P);
        Token *fname = expect(P, TOK_IDENT, "expected field name");
        expect(P, TOK_SEMICOLON, "expected ';' after field");
        int fn = new_node(NODE_VAR_DECL, fname->line);
        my_strcpy(rdx_nodes[fn].sval, fname->value);
        my_strcpy(rdx_nodes[fn].rdx_type, ftype->value);
        rdx_nodes[n].children[rdx_nodes[n].child_count++] = fn;
    }
    expect(P, TOK_RBRACE, "expected '}'");
    return n;
}

/* ---- Top-level program ------------------------------------- */
static int parse_program(Parser *P) {
    int root = new_node(NODE_PROGRAM, 0);
    while (!check(P, TOK_EOF)) {
        int n = -1;
        if (check(P, TOK_FN))        n = parse_fn_decl(P);
        else if (check(P, TOK_BLUEPRINT)) n = parse_blueprint_decl(P);
        else                          n = parse_stmt(P);
        if (n >= 0)
            rdx_nodes[root].children[rdx_nodes[root].child_count++] = n;
    }
    return root;
}

/* ---- Public API -------------------------------------------- */
void parser_init(Parser *P, Token *tokens, int count) {
    P->tokens    = tokens;
    P->count     = count;
    P->pos       = 0;
    P->had_error = 0;
}

int parser_parse(Parser *P) {
    rdx_node_count = 0;
    return parse_program(P);
}

/* Debug dump */
static const char *node_name(NodeType t) {
    static const char *names[] = {
        "PROGRAM","VAR_DECL","FN_DECL","RETURN",
        "IF","LOOP","BLUEPRINT","ALLOC","DROP",
        "ASSIGN","FN_CALL","SYS_CALL","BINARY_OP",
        "LIT_NUM","LIT_STR","IDENT","SYS_BLOCK",
        "MEMBER_ACCESS","BLOCK"
    };
    return (t >= 0 && t < 19) ? names[t] : "???";
}
void parser_dump(int idx, int depth) {
    if (idx < 0 || idx >= rdx_node_count) return;
    ASTNode *n = &rdx_nodes[idx];
    for (int i = 0; i < depth * 2; i++) rdx_print_char(' ');
    rdx_print(node_name(n->type));
    if (n->sval[0])      { rdx_print(" sval=\""); rdx_print(n->sval); rdx_print("\""); }
    if (n->ival)         { rdx_print(" ival=");   rdx_print_num(n->ival); }
    if (n->op[0])        { rdx_print(" op=");     rdx_print(n->op); }
    if (n->rdx_type[0])  { rdx_print(" type=");   rdx_print(n->rdx_type); }
    rdx_print("\r\n");
    if (n->condition >= 0) parser_dump(n->condition, depth + 1);
    if (n->left      >= 0) parser_dump(n->left,      depth + 1);
    if (n->right     >= 0) parser_dump(n->right,     depth + 1);
    if (n->body      >= 0) parser_dump(n->body,      depth + 1);
    if (n->else_body >= 0) parser_dump(n->else_body, depth + 1);
    for (int i = 0; i < n->child_count; i++) parser_dump(n->children[i], depth + 1);
}
