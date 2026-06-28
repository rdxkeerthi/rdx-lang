/* parser.h — RDX AST Parser: Public API */
#ifndef RDX_PARSER_H
#define RDX_PARSER_H
#include "lexer.h"

typedef enum {
    NODE_PROGRAM, NODE_VAR_DECL, NODE_FN_DECL, NODE_RETURN,
    NODE_IF, NODE_LOOP, NODE_BLUEPRINT, NODE_ALLOC, NODE_DROP,
    NODE_ASSIGN, NODE_FN_CALL, NODE_SYS_CALL, NODE_BINARY_OP,
    NODE_LITERAL_NUM, NODE_LITERAL_STR, NODE_IDENT,
    NODE_SYS_BLOCK, NODE_MEMBER_ACCESS, NODE_BLOCK,
} NodeType;

typedef struct {
    NodeType type;
    char     sval[256];     /* string payload / ident name   */
    i64      ival;          /* integer literal value         */
    char     op[4];         /* operator: +  -  *  /  ==  <  */
    char     rdx_type[64];  /* declared type name            */
    int      left;          /* child index or -1             */
    int      right;
    int      condition;
    int      body;
    int      else_body;
    int      children[64];  /* block stmts / fn params       */
    int      child_count;
    int      line;
} ASTNode;

/* Global node pool — filled by the parser */
extern ASTNode rdx_nodes[];
extern int     rdx_node_count;

typedef struct {
    Token *tokens;
    int    count;
    int    pos;
    int    had_error;
} Parser;

void parser_init (Parser *P, Token *tokens, int count);
int  parser_parse(Parser *P);           /* returns root index or -1 */
void parser_dump (int node, int depth); /* debug pretty-print       */
#endif
