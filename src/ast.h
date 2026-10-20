/*
 * ============================================================================
 *  ast.h  —  RDX Abstract Syntax Tree (Arena-Allocated)
 * ============================================================================
 */

#ifndef RDX_AST_H
#define RDX_AST_H

#include "core.h"
#include "token.h"
#include "arena.h"

typedef struct AstExpr AstExpr;
typedef struct AstStmt AstStmt;
typedef struct AstDecl AstDecl;

/* ============================================================
 *  Expressions
 * ============================================================ */
typedef enum {
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_STR,
    EXPR_BOOL,
    EXPR_NIL,
    EXPR_IDENT,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_MEMBER,
    EXPR_INDEX,
    EXPR_BIT_RANGE,
    EXPR_ARRAY,
    EXPR_BLOCK,
    EXPR_SPAWN,
    EXPR_YIELD,
    EXPR_CAP_REQ,
    EXPR_COMPTIME,
    EXPR_ASM,
} AstExprKind;

struct AstExpr {
    AstExprKind kind;
    u32         line;
    u32         col;
    union {
        i64 int_val;
        double float_val;
        struct {
            const char *chars;
            usize       length;
        } str_val;
        bool bool_val;
        struct {
            const char *name;
            usize       length;
        } ident;
        struct {
            TokenType  op;
            AstExpr   *operand;
        } unary;
        struct {
            TokenType  op;
            AstExpr   *left;
            AstExpr   *right;
        } binary;
        struct {
            TokenType  op;
            AstExpr   *target;
            AstExpr   *value;
        } assign;
        struct {
            AstExpr   *callee;
            AstExpr  **args;
            u32        arg_count;
        } call;
        struct {
            AstExpr    *target;
            const char *field;
            usize       field_len;
        } member;
        struct {
            AstExpr   *target;
            AstExpr   *index;
        } index;
        struct {
            AstExpr   *target;
            AstExpr   *high_bit;
            AstExpr   *low_bit;
        } bit_range;
        struct {
            AstExpr  **elements;
            u32        count;
        } array;
        struct {
            AstStmt  **stmts;
            u32        stmt_count;
            AstExpr   *result_expr;
        } block;
        struct {
            AstExpr   *call_expr;
        } spawn;
        struct {
            AstExpr   *value;
        } yield;
        struct {
            const char *cap_name;
            usize       cap_len;
        } cap_req;
        struct {
            AstExpr   *expr;
        } comptime_expr;
        struct {
            const char *instruction;
            usize       length;
            bool        is_volatile;
        } inline_asm;
    };
};

/* ============================================================
 *  Statements
 * ============================================================ */
typedef enum {
    STMT_EXPR,
    STMT_VAR,
    STMT_IF,
    STMT_LOOP,
    STMT_WHILE,
    STMT_FOR,
    STMT_RETURN,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_PRINT,
    STMT_ASM,
} AstStmtKind;

struct AstStmt {
    AstStmtKind kind;
    u32         line;
    u32         col;
    union {
        AstExpr *expr;
        struct {
            const char *name;
            usize       name_len;
            const char *type_name;
            usize       type_len;
            AstExpr    *init;
            bool        is_mut;
            bool        is_fix;
        } var_decl;
        struct {
            AstExpr   *condition;
            AstStmt   *then_branch;
            AstStmt   *else_branch;
        } if_stmt;
        struct {
            AstStmt   *body;
        } loop_stmt;
        struct {
            AstExpr   *condition;
            AstStmt   *body;
        } while_stmt;
        struct {
            const char *var_name;
            usize       var_len;
            AstExpr    *iterable;
            AstStmt    *body;
        } for_stmt;
        struct {
            AstExpr   *value;
        } return_stmt;
        struct {
            AstExpr  **exprs;
            u32        count;
        } print_stmt;
        struct {
            const char *instruction;
            usize       length;
            bool        is_volatile;
        } asm_stmt;
    };
};

/* ============================================================
 *  Declarations
 * ============================================================ */
typedef enum {
    DECL_FUNCTION,
    DECL_SHAPE,
    DECL_STRUCT,
    DECL_REG_BLOCK,
    DECL_ISR,
    DECL_STMT,
} AstDeclKind;

typedef struct {
    const char *name;
    usize       name_len;
    const char *type_name;
    usize       type_len;
} AstParam;

typedef struct {
    const char *name;
    usize       name_len;
    const char *type_name;
    usize       type_len;
} AstField;

typedef struct {
    const char *name;
    usize       name_len;
    const char *type_name;
    usize       type_len;
    u64         offset;
    bool        is_volatile;
} AstRegMember;

struct AstDecl {
    AstDeclKind kind;
    u32         line;
    u32         col;
    union {
        struct {
            const char *name;
            usize       name_len;
            AstParam   *params;
            u32         param_count;
            const char *return_type;
            usize       return_type_len;
            AstExpr    *body;
            bool        is_public;
        } fn_decl;
        struct {
            const char *name;
            usize       name_len;
            AstField   *fields;
            u32         field_count;
            bool        is_packed;
        } struct_decl;
        struct {
            const char *name;
            usize       name_len;
            AstField   *fields;
            u32         field_count;
        } shape_decl;
        struct {
            const char   *name;
            usize         name_len;
            u64           base_address;
            const char   *cap_token;
            usize         cap_len;
            AstRegMember *registers;
            u32           reg_count;
        } reg_block;
        struct {
            const char *name;
            usize       name_len;
            i32         vector_num;
            AstExpr    *body;
        } isr_decl;
        AstStmt *stmt;
    };
};

/* ============================================================
 *  Program AST Root
 * ============================================================ */
typedef struct {
    AstDecl **decls;
    u32       decl_count;
    Arena    *arena;
} AstProgram;

/* Factory Helpers */
AstExpr *ast_make_int(Arena *arena, i64 val, u32 line, u32 col);
AstExpr *ast_make_float(Arena *arena, double val, u32 line, u32 col);
AstExpr *ast_make_str(Arena *arena, const char *chars, usize length, u32 line, u32 col);
AstExpr *ast_make_bool(Arena *arena, bool val, u32 line, u32 col);
AstExpr *ast_make_nil(Arena *arena, u32 line, u32 col);
AstExpr *ast_make_ident(Arena *arena, const char *name, usize length, u32 line, u32 col);
AstExpr *ast_make_unary(Arena *arena, TokenType op, AstExpr *operand, u32 line, u32 col);
AstExpr *ast_make_binary(Arena *arena, TokenType op, AstExpr *left, AstExpr *right, u32 line, u32 col);
AstExpr *ast_make_call(Arena *arena, AstExpr *callee, AstExpr **args, u32 count, u32 line, u32 col);
AstExpr *ast_make_block(Arena *arena, AstStmt **stmts, u32 count, AstExpr *result, u32 line, u32 col);

#endif /* RDX_AST_H */
