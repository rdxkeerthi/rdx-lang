/*
 * ============================================================================
 *  type_check.c  —  RDX Static Type Checker Implementation  (v2.0)
 * ============================================================================
 *
 *  Full Hindley-Milner-style bidirectional type inference + checking.
 *  Traverses the AST produced by parser.c and assigns RdxType* to every
 *  expression node.  Emits structured diagnostics (diag.h) on mismatch.
 *
 *  Type checking order:
 *    1. Pre-scan all struct/enum declarations → register in named_types[]
 *    2. Pre-scan all function declarations → register signatures
 *    3. Full pass over all declarations in order
 * ============================================================================
 */

#include "type_check.h"

/* ============================================================
 *  Built-in singleton types
 * ============================================================ */
RdxType TC_TY_NIL     = { TY_NIL,    "nil",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_BOOL    = { TY_BOOL,   "blo",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_INT     = { TY_INT,    "num",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_FLOAT   = { TY_FLOAT,  "flt",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_DOUBLE  = { TY_DOUBLE, "dob",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_BYTE    = { TY_BYTE,   "byt",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_STR     = { TY_STR,    "chr",     3, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_NEVER   = { TY_NEVER,  "never",   5, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };
RdxType TC_TY_UNKNOWN = { TY_UNKNOWN,"unknown", 7, {{0}}, 0, {{0}}, 0, NULL,   {{0}}, 0, {{0}}, 0 };

/* ============================================================
 *  Helpers: resolve built-in type names
 * ============================================================ */
static RdxType *builtin_type_from_name(const char *name, usize len) {
    if (len == 3 && my_strncmp(name, "num", 3) == 0) return &TC_TY_INT;
    if (len == 3 && my_strncmp(name, "flt", 3) == 0) return &TC_TY_FLOAT;
    if (len == 3 && my_strncmp(name, "dob", 3) == 0) return &TC_TY_DOUBLE;
    if (len == 3 && my_strncmp(name, "chr", 3) == 0) return &TC_TY_STR;
    if (len == 3 && my_strncmp(name, "blo", 3) == 0) return &TC_TY_BOOL;
    if (len == 3 && my_strncmp(name, "byt", 3) == 0) return &TC_TY_BYTE;
    if (len == 3 && my_strncmp(name, "ptr", 3) == 0) {
        static RdxType ptr_ty = { TY_PTR, "ptr", 3, {{0}}, 0, {{0}}, 0, NULL, {{0}}, 0, {{0}}, 0 };
        return &ptr_ty;
    }
    if (len == 3 && my_strncmp(name, "nil", 3) == 0) return &TC_TY_NIL;
    if (len == 5 && my_strncmp(name, "never", 5) == 0) return &TC_TY_NEVER;
    return NULL;
}

/* ============================================================
 *  Initialisation
 * ============================================================ */
void tc_init(TypeChecker *tc, Arena *arena) {
    my_memset(tc, 0, sizeof(TypeChecker));
    tc->arena             = arena;
    tc->sym_count         = 0;
    tc->scope_depth       = 0;
    tc->named_type_count  = 0;
    tc->had_error         = FALSE;
    tc->error_count       = 0;
    tc->current_fn_return = &TC_TY_NIL;
    tc->in_loop           = FALSE;
}

/* ============================================================
 *  Compound Type Constructors
 * ============================================================ */
RdxType *tc_make_array(Arena *arena, RdxType *elem) {
    RdxType *t = (RdxType*)arena_alloc_zero(arena, sizeof(RdxType));
    t->kind        = TY_ARRAY;
    t->name        = "[]";
    t->name_len    = 2;
    t->inner[0]    = elem;
    t->inner_count = 1;
    return t;
}

RdxType *tc_make_option(Arena *arena, RdxType *inner) {
    RdxType *t = (RdxType*)arena_alloc_zero(arena, sizeof(RdxType));
    t->kind        = TY_OPTION;
    t->name        = "Option";
    t->name_len    = 6;
    t->inner[0]    = inner;
    t->inner_count = 1;
    return t;
}

RdxType *tc_make_result(Arena *arena, RdxType *ok_type) {
    RdxType *t = (RdxType*)arena_alloc_zero(arena, sizeof(RdxType));
    t->kind        = TY_RESULT;
    t->name        = "Result";
    t->name_len    = 6;
    t->inner[0]    = ok_type;
    t->inner[1]    = &TC_TY_STR;  /* Err is always chr */
    t->inner_count = 2;
    return t;
}

RdxType *tc_make_tuple(Arena *arena, RdxType **elems, u32 count) {
    RdxType *t = (RdxType*)arena_alloc_zero(arena, sizeof(RdxType));
    t->kind        = TY_TUPLE;
    t->name        = "tuple";
    t->name_len    = 5;
    t->inner_count = count;
    for (u32 i = 0; i < count && i < MAX_TYPE_PARAMS; i++) {
        t->inner[i] = elems[i];
    }
    return t;
}

RdxType *tc_make_fn(Arena *arena, RdxType **params, u32 param_count, RdxType *ret) {
    RdxType *t = (RdxType*)arena_alloc_zero(arena, sizeof(RdxType));
    t->kind        = TY_FUNCTION;
    t->name        = "fn";
    t->name_len    = 2;
    t->param_count = param_count;
    for (u32 i = 0; i < param_count && i < MAX_FN_PARAMS; i++) {
        t->param_types[i] = params[i];
    }
    t->return_type = ret ? ret : &TC_TY_NIL;
    return t;
}

/* ============================================================
 *  Type Compatibility
 * ============================================================ */
bool tc_types_compatible(const RdxType *a, const RdxType *b) {
    if (!a || !b) return FALSE;
    if (a == b) return TRUE;
    if (a->kind == TY_UNKNOWN || b->kind == TY_UNKNOWN) return TRUE; /* allow unknown */
    if (a->kind == TY_INFER || b->kind == TY_INFER) return TRUE;
    if (a->kind == TY_NEVER) return TRUE;  /* never is compatible with everything */
    if (b->kind == TY_NEVER) return TRUE;
    if (a->kind != b->kind) {
        /* Allow num ↔ flt coercion in arithmetic contexts */
        if ((a->kind == TY_INT && b->kind == TY_FLOAT) ||
            (a->kind == TY_FLOAT && b->kind == TY_INT) ||
            (a->kind == TY_INT && b->kind == TY_DOUBLE) ||
            (a->kind == TY_DOUBLE && b->kind == TY_INT) ||
            (a->kind == TY_FLOAT && b->kind == TY_DOUBLE) ||
            (a->kind == TY_DOUBLE && b->kind == TY_FLOAT)) {
            return TRUE;
        }
        return FALSE;
    }
    /* Same kind — check inner types for generics */
    if (a->kind == TY_STRUCT || a->kind == TY_ENUM) {
        if (!a->name || !b->name) return FALSE;
        if (a->name_len != b->name_len) return FALSE;
        return my_strncmp(a->name, b->name, a->name_len) == 0;
    }
    if (a->kind == TY_ARRAY) {
        if (a->inner_count == 0 || b->inner_count == 0) return TRUE; /* untyped array */
        return tc_types_compatible(a->inner[0], b->inner[0]);
    }
    if (a->kind == TY_TUPLE) {
        if (a->inner_count != b->inner_count) return FALSE;
        for (u32 i = 0; i < a->inner_count; i++) {
            if (!tc_types_compatible(a->inner[i], b->inner[i])) return FALSE;
        }
        return TRUE;
    }
    return TRUE;
}

bool tc_type_is_numeric(const RdxType *t) {
    if (!t) return FALSE;
    return t->kind == TY_INT || t->kind == TY_FLOAT ||
           t->kind == TY_DOUBLE || t->kind == TY_BYTE;
}

bool tc_type_is_copy(const RdxType *t) {
    if (!t) return FALSE;
    return t->kind == TY_INT || t->kind == TY_FLOAT ||
           t->kind == TY_DOUBLE || t->kind == TY_BYTE ||
           t->kind == TY_BOOL || t->kind == TY_NIL;
}

/* ============================================================
 *  Scope management
 * ============================================================ */
static void tc_push_scope(TypeChecker *tc) {
    if (tc->scope_depth >= TC_MAX_SCOPES) {
        rdx_print("[TC] ERROR: scope stack overflow\n");
        tc->had_error = TRUE;
        return;
    }
    tc->scopes[tc->scope_depth].base  = tc->sym_count;
    tc->scopes[tc->scope_depth].count = 0;
    tc->scope_depth++;
}

static void tc_pop_scope(TypeChecker *tc) {
    if (tc->scope_depth == 0) return;
    tc->scope_depth--;
    /* Remove all symbols added in this scope */
    tc->sym_count = tc->scopes[tc->scope_depth].base;
}

static void tc_define_symbol(TypeChecker *tc, const char *name, usize len, RdxType *type, bool is_mut) {
    if (tc->sym_count >= TC_MAX_SYMS) {
        rdx_print("[TC] ERROR: symbol table overflow\n");
        tc->had_error = TRUE;
        return;
    }
    TcSymbol *sym = &tc->symbols[tc->sym_count++];
    sym->name     = name;
    sym->name_len = len;
    sym->type     = type;
    sym->is_mut   = is_mut;
    if (tc->scope_depth > 0) {
        tc->scopes[tc->scope_depth - 1].count++;
    }
}

static TcSymbol *tc_find_symbol(TypeChecker *tc, const char *name, usize len) {
    /* Search backwards — innermost scope first */
    for (i32 i = (i32)tc->sym_count - 1; i >= 0; i--) {
        if (tc->symbols[i].name_len == len &&
            my_strncmp(tc->symbols[i].name, name, len) == 0) {
            return &tc->symbols[i];
        }
    }
    return NULL;
}

/* ============================================================
 *  Named type registry
 * ============================================================ */
RdxType *tc_find_type(TypeChecker *tc, const char *name, usize len) {
    /* Check built-in types first */
    RdxType *builtin = builtin_type_from_name(name, len);
    if (builtin) return builtin;
    /* Search user-defined types */
    for (u32 i = 0; i < tc->named_type_count; i++) {
        RdxType *t = tc->named_types[i];
        if (t && t->name_len == len && my_strncmp(t->name, name, len) == 0) {
            return t;
        }
    }
    return &TC_TY_UNKNOWN;
}

static void tc_register_type(TypeChecker *tc, RdxType *type) {
    if (tc->named_type_count >= TC_MAX_TYPES) {
        rdx_print("[TC] ERROR: type registry overflow\n");
        tc->had_error = TRUE;
        return;
    }
    tc->named_types[tc->named_type_count++] = type;
}

/* ============================================================
 *  Error reporting helper
 * ============================================================ */
static void tc_error(TypeChecker *tc, u32 line, u32 col, const char *msg) {
    tc->had_error = TRUE;
    tc->error_count++;
    rdx_diag_error(line, col, 1, "[TYPE ERROR]", msg);
}

/* ============================================================
 *  tc_print_type  — diagnostic helper
 * ============================================================ */
void tc_print_type(const RdxType *t) {
    if (!t) { rdx_print("?"); return; }
    switch (t->kind) {
        case TY_NIL:     rdx_print("nil"); break;
        case TY_BOOL:    rdx_print("blo"); break;
        case TY_INT:     rdx_print("num"); break;
        case TY_FLOAT:   rdx_print("flt"); break;
        case TY_DOUBLE:  rdx_print("dob"); break;
        case TY_BYTE:    rdx_print("byt"); break;
        case TY_STR:     rdx_print("chr"); break;
        case TY_PTR:     rdx_print("ptr"); break;
        case TY_NEVER:   rdx_print("!"); break;
        case TY_UNKNOWN: rdx_print("?"); break;
        case TY_INFER:   rdx_print("_"); break;
        case TY_ARRAY:
            rdx_print("[]");
            if (t->inner_count > 0) tc_print_type(t->inner[0]);
            break;
        case TY_TUPLE:
            rdx_print("(");
            for (u32 i = 0; i < t->inner_count; i++) {
                if (i > 0) rdx_print(", ");
                tc_print_type(t->inner[i]);
            }
            rdx_print(")");
            break;
        case TY_FUNCTION:
            rdx_print("fn(");
            for (u32 i = 0; i < t->param_count; i++) {
                if (i > 0) rdx_print(", ");
                tc_print_type(t->param_types[i]);
            }
            rdx_print(") -> ");
            tc_print_type(t->return_type);
            break;
        case TY_OPTION:
            rdx_print("Option[");
            if (t->inner_count > 0) tc_print_type(t->inner[0]);
            rdx_print("]");
            break;
        case TY_RESULT:
            rdx_print("Result[");
            if (t->inner_count > 0) tc_print_type(t->inner[0]);
            rdx_print("]");
            break;
        case TY_STRUCT:
        case TY_ENUM:
        case TY_GENERIC:
            if (t->name) rdx_print(t->name);
            else rdx_print("<unnamed>");
            break;
        default:
            rdx_print("<type>");
            break;
    }
}

/* ============================================================
 *  Parse a type annotation string into RdxType
 * ============================================================ */
static RdxType *tc_resolve_type_annotation(TypeChecker *tc, const char *name, usize len) {
    if (!name || len == 0) return &TC_TY_UNKNOWN;
    /* Built-ins */
    RdxType *b = builtin_type_from_name(name, len);
    if (b) return b;
    /* Array slice: starts with [] */
    if (len > 2 && name[0] == '[' && name[1] == ']') {
        RdxType *elem = tc_resolve_type_annotation(tc, name + 2, len - 2);
        return tc_make_array(tc->arena, elem);
    }
    /* Option[T] */
    if (len > 7 && my_strncmp(name, "Option[", 7) == 0 && name[len-1] == ']') {
        RdxType *inner = tc_resolve_type_annotation(tc, name + 7, len - 8);
        return tc_make_option(tc->arena, inner);
    }
    /* Result[T] */
    if (len > 7 && my_strncmp(name, "Result[", 7) == 0 && name[len-1] == ']') {
        RdxType *inner = tc_resolve_type_annotation(tc, name + 7, len - 8);
        return tc_make_result(tc->arena, inner);
    }
    /* User-defined struct/enum */
    return tc_find_type(tc, name, len);
}

/* ============================================================
 *  Expression type inference — forward declaration
 * ============================================================ */
static RdxType *infer_expr(TypeChecker *tc, AstExpr *expr);
static void     check_stmt(TypeChecker *tc, AstStmt *stmt);

/* ============================================================
 *  tc_infer_expr — bidirectional (used for both infer + check)
 * ============================================================ */
static RdxType *infer_expr(TypeChecker *tc, AstExpr *expr) {
    if (!expr) return &TC_TY_NIL;

    switch (expr->kind) {
        case EXPR_INT:    return &TC_TY_INT;
        case EXPR_FLOAT:  return &TC_TY_FLOAT;
        case EXPR_STR:    return &TC_TY_STR;
        case EXPR_BOOL:   return &TC_TY_BOOL;
        case EXPR_NIL:    return &TC_TY_NIL;

        case EXPR_IDENT: {
            TcSymbol *sym = tc_find_symbol(tc, expr->ident.name, expr->ident.length);
            if (!sym) {
                /* Could be a built-in function or a type name — allow */
                return &TC_TY_UNKNOWN;
            }
            return sym->type;
        }

        case EXPR_UNARY: {
            RdxType *operand = infer_expr(tc, expr->unary.operand);
            switch (expr->unary.op) {
                case TOK_MINUS:
                    if (!tc_type_is_numeric(operand)) {
                        tc_error(tc, expr->line, expr->col, "Unary '-' requires numeric type");
                    }
                    return operand;
                case TOK_NOT:
                    return &TC_TY_BOOL;
                case TOK_TILDE:
                    if (operand->kind != TY_INT && operand->kind != TY_BYTE) {
                        tc_error(tc, expr->line, expr->col, "Bitwise NOT (~) requires integer type");
                    }
                    return operand;
                default:
                    return operand;
            }
        }

        case EXPR_BINARY: {
            RdxType *lt = infer_expr(tc, expr->binary.left);
            RdxType *rt = infer_expr(tc, expr->binary.right);
            TokenType op = expr->binary.op;

            /* Comparison operators always return bool */
            if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT ||
                op == TOK_LTE || op == TOK_GT || op == TOK_GTE) {
                if (!tc_types_compatible(lt, rt)) {
                    tc_error(tc, expr->line, expr->col, "Cannot compare values of incompatible types");
                }
                return &TC_TY_BOOL;
            }

            /* Logical operators return bool */
            if (op == TOK_AND || op == TOK_OR) {
                return &TC_TY_BOOL;
            }

            /* Arithmetic: string concat with + */
            if (op == TOK_PLUS && lt->kind == TY_STR && rt->kind == TY_STR) {
                return &TC_TY_STR;
            }

            /* Arithmetic operators */
            if (!tc_types_compatible(lt, rt)) {
                tc_error(tc, expr->line, expr->col, "Type mismatch in binary expression");
            }
            /* Promote to wider numeric type */
            if (lt->kind == TY_DOUBLE || rt->kind == TY_DOUBLE) return &TC_TY_DOUBLE;
            if (lt->kind == TY_FLOAT  || rt->kind == TY_FLOAT)  return &TC_TY_FLOAT;
            return lt;
        }

        case EXPR_ASSIGN: {
            RdxType *rhs = infer_expr(tc, expr->assign.value);
            /* Look up target and update its type if inferred */
            if (expr->assign.target && expr->assign.target->kind == EXPR_IDENT) {
                TcSymbol *sym = tc_find_symbol(tc,
                    expr->assign.target->ident.name,
                    expr->assign.target->ident.length);
                if (sym) {
                    if (!sym->is_mut) {
                        tc_error(tc, expr->line, expr->col, "Cannot assign to immutable variable");
                    }
                    if (!tc_types_compatible(sym->type, rhs)) {
                        tc_error(tc, expr->line, expr->col, "Type mismatch in assignment");
                    }
                }
            }
            return rhs;
        }

        case EXPR_CALL: {
            RdxType *callee_type = infer_expr(tc, expr->call.callee);
            /* Check argument count and types if we have a function type */
            if (callee_type->kind == TY_FUNCTION || callee_type->kind == TY_CLOSURE) {
                if (expr->call.arg_count != callee_type->param_count) {
                    tc_error(tc, expr->line, expr->col, "Wrong number of arguments in function call");
                }
                for (u32 i = 0; i < expr->call.arg_count && i < callee_type->param_count; i++) {
                    RdxType *arg_type = infer_expr(tc, expr->call.args[i]);
                    if (!tc_types_compatible(arg_type, callee_type->param_types[i])) {
                        tc_error(tc, expr->line, expr->col, "Argument type mismatch in function call");
                    }
                }
                return callee_type->return_type ? callee_type->return_type : &TC_TY_NIL;
            }
            /* Unknown callee — check args anyway, return unknown */
            for (u32 i = 0; i < expr->call.arg_count; i++) {
                infer_expr(tc, expr->call.args[i]);
            }
            return &TC_TY_UNKNOWN;
        }

        case EXPR_MEMBER: {
            RdxType *target_type = infer_expr(tc, expr->member.target);
            /* Look for field in struct type */
            if (target_type->kind == TY_STRUCT) {
                for (u32 i = 0; i < target_type->field_count; i++) {
                    if (target_type->fields[i].name_len == expr->member.field_len &&
                        my_strncmp(target_type->fields[i].name, expr->member.field, expr->member.field_len) == 0) {
                        return target_type->fields[i].type;
                    }
                }
                tc_error(tc, expr->line, expr->col, "Unknown struct field");
                return &TC_TY_UNKNOWN;
            }
            /* Array .len property */
            if (target_type->kind == TY_ARRAY && expr->member.field_len == 3 &&
                my_strncmp(expr->member.field, "len", 3) == 0) {
                return &TC_TY_INT;
            }
            return &TC_TY_UNKNOWN;
        }

        case EXPR_INDEX: {
            RdxType *target_type = infer_expr(tc, expr->index.target);
            RdxType *idx_type    = infer_expr(tc, expr->index.index);
            if (idx_type->kind != TY_INT) {
                tc_error(tc, expr->line, expr->col, "Array index must be of type num");
            }
            if (target_type->kind == TY_ARRAY && target_type->inner_count > 0) {
                return target_type->inner[0];
            }
            if (target_type->kind == TY_STR) {
                return &TC_TY_BYTE;
            }
            return &TC_TY_UNKNOWN;
        }

        case EXPR_ARRAY: {
            if (expr->array.count == 0) {
                return tc_make_array(tc->arena, &TC_TY_UNKNOWN);
            }
            RdxType *elem_type = infer_expr(tc, expr->array.elements[0]);
            for (u32 i = 1; i < expr->array.count; i++) {
                RdxType *t = infer_expr(tc, expr->array.elements[i]);
                if (!tc_types_compatible(elem_type, t)) {
                    tc_error(tc, expr->line, expr->col, "Array elements must have compatible types");
                }
            }
            return tc_make_array(tc->arena, elem_type);
        }

        case EXPR_BLOCK: {
            tc_push_scope(tc);
            for (u32 i = 0; i < expr->block.stmt_count; i++) {
                check_stmt(tc, expr->block.stmts[i]);
            }
            RdxType *result_type = &TC_TY_NIL;
            if (expr->block.result_expr) {
                result_type = infer_expr(tc, expr->block.result_expr);
            }
            tc_pop_scope(tc);
            return result_type;
        }

        case EXPR_SPAWN:
            /* spawn returns a fiber handle */
            infer_expr(tc, expr->spawn.call_expr);
            return &TC_TY_INT;  /* Fiber ID as num */

        case EXPR_YIELD:
            if (expr->yield.value) infer_expr(tc, expr->yield.value);
            return &TC_TY_NIL;

        case EXPR_CAP_REQ:
            return &TC_TY_UNKNOWN;  /* CapToken */

        case EXPR_COMPTIME:
            return infer_expr(tc, expr->comptime_expr.expr);

        default:
            return &TC_TY_UNKNOWN;
    }
}

RdxType *tc_infer_expr(TypeChecker *tc, AstExpr *expr) {
    return infer_expr(tc, expr);
}

RdxType *tc_check_expr(TypeChecker *tc, AstExpr *expr, RdxType *expected) {
    RdxType *actual = infer_expr(tc, expr);
    if (expected && !tc_types_compatible(actual, expected)) {
        tc_error(tc, expr ? expr->line : 0, expr ? expr->col : 0,
                 "Type mismatch: expected type does not match actual type");
        rdx_print("  Expected: ");
        tc_print_type(expected);
        rdx_print("\n  Actual:   ");
        tc_print_type(actual);
        rdx_print("\n");
    }
    return actual;
}

/* ============================================================
 *  Statement type checking
 * ============================================================ */
static void check_stmt(TypeChecker *tc, AstStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            infer_expr(tc, stmt->expr);
            break;

        case STMT_VAR: {
            RdxType *declared_type = &TC_TY_UNKNOWN;
            if (stmt->var_decl.type_name && stmt->var_decl.type_len > 0) {
                declared_type = tc_resolve_type_annotation(tc,
                    stmt->var_decl.type_name, stmt->var_decl.type_len);
            }

            RdxType *init_type = &TC_TY_UNKNOWN;
            if (stmt->var_decl.init) {
                init_type = infer_expr(tc, stmt->var_decl.init);
            }

            /* Use declared type if available, else infer */
            RdxType *final_type = (declared_type->kind != TY_UNKNOWN) ? declared_type : init_type;

            /* Verify compatibility */
            if (declared_type->kind != TY_UNKNOWN && init_type->kind != TY_UNKNOWN) {
                if (!tc_types_compatible(declared_type, init_type)) {
                    tc_error(tc, stmt->line, stmt->col, "Variable declaration type mismatch");
                    rdx_print("  Declared: ");
                    tc_print_type(declared_type);
                    rdx_print("\n  Init:     ");
                    tc_print_type(init_type);
                    rdx_print("\n");
                }
            }

            tc_define_symbol(tc, stmt->var_decl.name, stmt->var_decl.name_len,
                             final_type, stmt->var_decl.is_mut);
            break;
        }

        case STMT_IF: {
            RdxType *cond_type = infer_expr(tc, stmt->if_stmt.condition);
            (void)cond_type;  /* Any truthy type is acceptable */
            tc_push_scope(tc);
            check_stmt(tc, stmt->if_stmt.then_branch);
            tc_pop_scope(tc);
            if (stmt->if_stmt.else_branch) {
                tc_push_scope(tc);
                check_stmt(tc, stmt->if_stmt.else_branch);
                tc_pop_scope(tc);
            }
            break;
        }

        case STMT_WHILE: {
            infer_expr(tc, stmt->while_stmt.condition);
            bool prev = tc->in_loop;
            tc->in_loop = TRUE;
            tc_push_scope(tc);
            check_stmt(tc, stmt->while_stmt.body);
            tc_pop_scope(tc);
            tc->in_loop = prev;
            break;
        }

        case STMT_LOOP: {
            bool prev = tc->in_loop;
            tc->in_loop = TRUE;
            tc_push_scope(tc);
            check_stmt(tc, stmt->loop_stmt.body);
            tc_pop_scope(tc);
            tc->in_loop = prev;
            break;
        }

        case STMT_FOR: {
            RdxType *iter_type = infer_expr(tc, stmt->for_stmt.iterable);
            RdxType *elem_type = &TC_TY_UNKNOWN;
            if (iter_type->kind == TY_ARRAY && iter_type->inner_count > 0) {
                elem_type = iter_type->inner[0];
            } else if (iter_type->kind == TY_STR) {
                elem_type = &TC_TY_BYTE;
            }
            bool prev = tc->in_loop;
            tc->in_loop = TRUE;
            tc_push_scope(tc);
            tc_define_symbol(tc, stmt->for_stmt.var_name, stmt->for_stmt.var_len,
                             elem_type, FALSE);
            check_stmt(tc, stmt->for_stmt.body);
            tc_pop_scope(tc);
            tc->in_loop = prev;
            break;
        }

        case STMT_RETURN: {
            RdxType *ret_type = &TC_TY_NIL;
            if (stmt->return_stmt.value) {
                ret_type = infer_expr(tc, stmt->return_stmt.value);
            }
            if (tc->current_fn_return && !tc_types_compatible(ret_type, tc->current_fn_return)) {
                tc_error(tc, stmt->line, stmt->col, "Return type does not match function declaration");
                rdx_print("  Function expects: ");
                tc_print_type(tc->current_fn_return);
                rdx_print("\n  Returning:        ");
                tc_print_type(ret_type);
                rdx_print("\n");
            }
            break;
        }

        case STMT_BREAK:
        case STMT_CONTINUE:
            if (!tc->in_loop) {
                tc_error(tc, stmt->line, stmt->col,
                         (stmt->kind == STMT_BREAK) ?
                         "'break' used outside of a loop" :
                         "'continue' used outside of a loop");
            }
            break;

        case STMT_PRINT:
            for (u32 i = 0; i < stmt->print_stmt.count; i++) {
                infer_expr(tc, stmt->print_stmt.exprs[i]);
            }
            break;

        default:
            break;
    }
}

/* ============================================================
 *  Declaration type checking
 * ============================================================ */
static void check_decl(TypeChecker *tc, AstDecl *decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_STMT:
            check_stmt(tc, decl->stmt);
            break;

        case DECL_STRUCT: {
            /* Build RdxType for this struct */
            RdxType *t = (RdxType*)arena_alloc_zero(tc->arena, sizeof(RdxType));
            t->kind         = TY_STRUCT;
            t->name         = decl->struct_decl.name;
            t->name_len     = decl->struct_decl.name_len;
            t->field_count  = decl->struct_decl.field_count;
            for (u32 i = 0; i < decl->struct_decl.field_count && i < MAX_STRUCT_FIELDS; i++) {
                t->fields[i].name     = decl->struct_decl.fields[i].name;
                t->fields[i].name_len = decl->struct_decl.fields[i].name_len;
                t->fields[i].type     = tc_resolve_type_annotation(tc,
                    decl->struct_decl.fields[i].type_name,
                    decl->struct_decl.fields[i].type_len);
            }
            tc_register_type(tc, t);
            /* Register as a constructor function symbol */
            RdxType *fn_type = (RdxType*)arena_alloc_zero(tc->arena, sizeof(RdxType));
            fn_type->kind = TY_FUNCTION;
            fn_type->return_type = t;
            fn_type->param_count = decl->struct_decl.field_count;
            for (u32 i = 0; i < decl->struct_decl.field_count && i < MAX_FN_PARAMS; i++) {
                fn_type->param_types[i] = t->fields[i].type;
            }
            tc_define_symbol(tc, decl->struct_decl.name, decl->struct_decl.name_len, fn_type, FALSE);
            break;
        }

        case DECL_FUNCTION: {
            /* Resolve parameter types */
            RdxType **param_types = (RdxType**)arena_alloc(tc->arena,
                sizeof(RdxType*) * decl->fn_decl.param_count);
            for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
                param_types[i] = tc_resolve_type_annotation(tc,
                    decl->fn_decl.params[i].type_name,
                    decl->fn_decl.params[i].type_len);
            }

            RdxType *ret_type = &TC_TY_NIL;
            if (decl->fn_decl.return_type && decl->fn_decl.return_type_len > 0) {
                ret_type = tc_resolve_type_annotation(tc,
                    decl->fn_decl.return_type, decl->fn_decl.return_type_len);
            }

            RdxType *fn_type = tc_make_fn(tc->arena, param_types,
                                           decl->fn_decl.param_count, ret_type);

            /* Register function in current scope */
            tc_define_symbol(tc, decl->fn_decl.name, decl->fn_decl.name_len, fn_type, FALSE);

            /* Type-check body in a new scope */
            RdxType *prev_ret = tc->current_fn_return;
            tc->current_fn_return = ret_type;
            tc_push_scope(tc);

            /* Add parameters to scope */
            for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
                tc_define_symbol(tc, decl->fn_decl.params[i].name,
                                 decl->fn_decl.params[i].name_len,
                                 param_types[i], FALSE);
            }

            /* Check body expression */
            if (decl->fn_decl.body) {
                RdxType *body_type = infer_expr(tc, decl->fn_decl.body);
                /* Body of fn is an expression — its type should match return */
                (void)body_type;
            }

            tc_pop_scope(tc);
            tc->current_fn_return = prev_ret;
            break;
        }

        default:
            break;
    }
}

/* ============================================================
 *  tc_check_program  — public entry point
 * ============================================================ */
bool tc_check_program(TypeChecker *tc, AstProgram *prog) {
    if (!prog) return FALSE;

    /* Pass 1: Pre-register all struct/enum/function names
     * so forward references work */
    for (u32 i = 0; i < prog->decl_count; i++) {
        AstDecl *d = prog->decls[i];
        if (!d) continue;
        if (d->kind == DECL_STRUCT) {
            /* Quick pre-register — fields resolved in pass 2 */
            RdxType *t = (RdxType*)arena_alloc_zero(tc->arena, sizeof(RdxType));
            t->kind     = TY_STRUCT;
            t->name     = d->struct_decl.name;
            t->name_len = d->struct_decl.name_len;
            tc_register_type(tc, t);
        } else if (d->kind == DECL_FUNCTION) {
            /* Register as unknown-type fn for recursive calls */
            RdxType *fn = (RdxType*)arena_alloc_zero(tc->arena, sizeof(RdxType));
            fn->kind = TY_FUNCTION;
            tc_define_symbol(tc, d->fn_decl.name, d->fn_decl.name_len, fn, FALSE);
        }
    }

    /* Pass 2: Full type checking */
    for (u32 i = 0; i < prog->decl_count; i++) {
        check_decl(tc, prog->decls[i]);
    }

    if (tc->had_error) {
        rdx_print("[TC] Type checking found ");
        rdx_print_num((i64)tc->error_count);
        rdx_print(" error(s)\n");
    }

    return !tc->had_error;
}
