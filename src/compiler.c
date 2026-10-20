/*
 * ============================================================================
 *  compiler.c  —  RDX AST-to-Bytecode Compiler Implementation
 * ============================================================================
 */

#include "compiler.h"

static void emit_byte(Compiler *c, u8 byte, u32 line) {
    chunk_write(c->chunk, byte, line);
}

static void emit_bytes(Compiler *c, u8 b1, u8 b2, u32 line) {
    emit_byte(c, b1, line);
    emit_byte(c, b2, line);
}

static void emit_constant(Compiler *c, RdxValue val, u32 line) {
    chunk_write_constant(c->chunk, val, line);
}

static usize emit_jump(Compiler *c, u8 instruction, u32 line) {
    emit_byte(c, instruction, line);
    emit_byte(c, 0xFF, line);
    emit_byte(c, 0xFF, line);
    return c->chunk->count - 2;
}

static void patch_jump(Compiler *c, usize offset) {
    usize jump = c->chunk->count - offset - 2;
    if (jump > 0xFFFF) {
        rdx_print("[ERROR] Jump payload too large\n");
        c->had_error = TRUE;
        return;
    }
    c->chunk->code[offset]     = (u8)(jump & 0xFF);
    c->chunk->code[offset + 1] = (u8)((jump >> 8) & 0xFF);
}

static void emit_loop(Compiler *c, usize loop_start, u32 line) {
    emit_byte(c, (u8)OP_LOOP, line);
    usize jump = c->chunk->count - loop_start + 2;
    if (jump > 0xFFFF) {
        rdx_print("[ERROR] Loop body too large\n");
        c->had_error = TRUE;
        return;
    }
    emit_byte(c, (u8)(jump & 0xFF), line);
    emit_byte(c, (u8)((jump >> 8) & 0xFF), line);
}

void compiler_init(Compiler *compiler, Compiler *enclosing, ObjFunction *function, Chunk *chunk, Arena *arena) {
    compiler->enclosing    = enclosing;
    compiler->function     = function;
    compiler->chunk        = chunk;
    compiler->local_count  = 0;
    compiler->scope_depth  = 0;
    compiler->current_loop = NULL;
    compiler->arena        = arena;
    compiler->had_error    = FALSE;

    /* Reserve slot 0 for the function/fiber context */
    Local *local = &compiler->locals[compiler->local_count++];
    local->depth  = 0;
    local->name   = "";
    local->length = 0;
    local->is_mut = FALSE;
}

static void begin_scope(Compiler *c) {
    c->scope_depth++;
}

static void end_scope(Compiler *c, u32 line) {
    c->scope_depth--;
    while (c->local_count > 0 && c->locals[c->local_count - 1].depth > c->scope_depth) {
        emit_byte(c, (u8)OP_POP, line);
        c->local_count--;
    }
}

static i32 resolve_local(Compiler *c, const char *name, usize length) {
    for (i32 i = (i32)c->local_count - 1; i >= 0; i--) {
        Local *local = &c->locals[i];
        if (local->length == length && my_strncmp(local->name, name, length) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_local(Compiler *c, const char *name, usize length, bool is_mut) {
    if (c->local_count >= MAX_LOCALS) {
        rdx_print("[ERROR] Too many local variables in scope\n");
        c->had_error = TRUE;
        return;
    }
    Local *local = &c->locals[c->local_count++];
    local->name   = name;
    local->length = length;
    local->depth  = c->scope_depth;
    local->is_mut = is_mut;
}

/* Forward Declarations */
static void compile_expr(Compiler *c, AstExpr *expr);
static void compile_stmt(Compiler *c, AstStmt *stmt);

static void compile_expr(Compiler *c, AstExpr *expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_INT:
            emit_constant(c, val_int(expr->int_val), expr->line);
            break;

        case EXPR_FLOAT:
            emit_constant(c, val_float(expr->float_val), expr->line);
            break;

        case EXPR_STR: {
            ObjString *str = obj_string_create_unescaped(c->arena, expr->str_val.chars, expr->str_val.length);
            emit_constant(c, val_str(str), expr->line);
            break;
        }

        case EXPR_BOOL:
            emit_byte(c, expr->bool_val ? (u8)OP_TRUE : (u8)OP_FALSE, expr->line);
            break;

        case EXPR_NIL:
            emit_byte(c, (u8)OP_NIL, expr->line);
            break;

        case EXPR_IDENT: {
            i32 arg = resolve_local(c, expr->ident.name, expr->ident.length);
            if (arg != -1) {
                emit_byte(c, (u8)OP_GET_LOCAL, expr->line);
                emit_byte(c, (u8)(arg & 0xFF), expr->line);
                emit_byte(c, (u8)((arg >> 8) & 0xFF), expr->line);
            } else {
                ObjString *name_str = obj_string_create(c->arena, expr->ident.name, expr->ident.length);
                u16 name_idx = chunk_add_constant(c->chunk, val_str(name_str));
                emit_byte(c, (u8)OP_GET_GLOBAL, expr->line);
                emit_byte(c, (u8)(name_idx & 0xFF), expr->line);
                emit_byte(c, (u8)((name_idx >> 8) & 0xFF), expr->line);
            }
            break;
        }

        case EXPR_UNARY:
            compile_expr(c, expr->unary.operand);
            switch (expr->unary.op) {
                case TOK_MINUS: emit_byte(c, (u8)OP_NEG, expr->line); break;
                case TOK_NOT:   emit_byte(c, (u8)OP_NOT, expr->line); break;
                case TOK_TILDE: emit_byte(c, (u8)OP_BIT_NOT, expr->line); break;
                default: break;
            }
            break;

        case EXPR_BINARY:
            if (expr->binary.op == TOK_AND) {
                compile_expr(c, expr->binary.left);
                usize end_jump = emit_jump(c, (u8)OP_JUMP_IF_FALSE, expr->line);
                emit_byte(c, (u8)OP_POP, expr->line);
                compile_expr(c, expr->binary.right);
                patch_jump(c, end_jump);
                return;
            }
            if (expr->binary.op == TOK_OR) {
                compile_expr(c, expr->binary.left);
                usize else_jump = emit_jump(c, (u8)OP_JUMP_IF_FALSE, expr->line);
                usize end_jump  = emit_jump(c, (u8)OP_JUMP, expr->line);
                patch_jump(c, else_jump);
                emit_byte(c, (u8)OP_POP, expr->line);
                compile_expr(c, expr->binary.right);
                patch_jump(c, end_jump);
                return;
            }

            compile_expr(c, expr->binary.left);
            compile_expr(c, expr->binary.right);

            switch (expr->binary.op) {
                case TOK_PLUS:     emit_byte(c, (u8)OP_ADD, expr->line); break;
                case TOK_MINUS:    emit_byte(c, (u8)OP_SUB, expr->line); break;
                case TOK_STAR:     emit_byte(c, (u8)OP_MUL, expr->line); break;
                case TOK_SLASH:    emit_byte(c, (u8)OP_DIV, expr->line); break;
                case TOK_PERCENT:  emit_byte(c, (u8)OP_MOD, expr->line); break;
                case TOK_EQ:       emit_byte(c, (u8)OP_EQ, expr->line); break;
                case TOK_NEQ:      emit_byte(c, (u8)OP_NEQ, expr->line); break;
                case TOK_LT:       emit_byte(c, (u8)OP_LT, expr->line); break;
                case TOK_LTE:      emit_byte(c, (u8)OP_LTE, expr->line); break;
                case TOK_GT:       emit_byte(c, (u8)OP_GT, expr->line); break;
                case TOK_GTE:      emit_byte(c, (u8)OP_GTE, expr->line); break;
                case TOK_AMP:      emit_byte(c, (u8)OP_BIT_AND, expr->line); break;
                case TOK_PIPE:     emit_byte(c, (u8)OP_BIT_OR, expr->line); break;
                case TOK_CARET:    emit_byte(c, (u8)OP_BIT_XOR, expr->line); break;
                case TOK_SHL:      emit_byte(c, (u8)OP_SHL, expr->line); break;
                case TOK_SHR:      emit_byte(c, (u8)OP_SHR, expr->line); break;
                default: break;
            }
            break;

        case EXPR_ASSIGN: {
            if (expr->assign.target->kind != EXPR_IDENT) {
                rdx_print("[ERROR] Invalid assignment target\n");
                c->had_error = TRUE;
                return;
            }

            AstExpr *target = expr->assign.target;
            i32 arg = resolve_local(c, target->ident.name, target->ident.length);

            if (expr->assign.op != TOK_ASSIGN) {
                /* Compound assignment: target = target OP value */
                if (arg != -1) {
                    emit_byte(c, (u8)OP_GET_LOCAL, expr->line);
                    emit_byte(c, (u8)(arg & 0xFF), expr->line);
                    emit_byte(c, (u8)((arg >> 8) & 0xFF), expr->line);
                } else {
                    ObjString *name_str = obj_string_create(c->arena, target->ident.name, target->ident.length);
                    u16 name_idx = chunk_add_constant(c->chunk, val_str(name_str));
                    emit_byte(c, (u8)OP_GET_GLOBAL, expr->line);
                    emit_byte(c, (u8)(name_idx & 0xFF), expr->line);
                    emit_byte(c, (u8)((name_idx >> 8) & 0xFF), expr->line);
                }
                compile_expr(c, expr->assign.value);

                switch (expr->assign.op) {
                    case TOK_PLUS_ASSIGN:  emit_byte(c, (u8)OP_ADD, expr->line); break;
                    case TOK_MINUS_ASSIGN: emit_byte(c, (u8)OP_SUB, expr->line); break;
                    case TOK_STAR_ASSIGN:  emit_byte(c, (u8)OP_MUL, expr->line); break;
                    case TOK_SLASH_ASSIGN: emit_byte(c, (u8)OP_DIV, expr->line); break;
                    default: break;
                }
            } else {
                compile_expr(c, expr->assign.value);
            }

            if (arg != -1) {
                emit_byte(c, (u8)OP_SET_LOCAL, expr->line);
                emit_byte(c, (u8)(arg & 0xFF), expr->line);
                emit_byte(c, (u8)((arg >> 8) & 0xFF), expr->line);
            } else {
                ObjString *name_str = obj_string_create(c->arena, target->ident.name, target->ident.length);
                u16 name_idx = chunk_add_constant(c->chunk, val_str(name_str));
                emit_byte(c, (u8)OP_SET_GLOBAL, expr->line);
                emit_byte(c, (u8)(name_idx & 0xFF), expr->line);
                emit_byte(c, (u8)((name_idx >> 8) & 0xFF), expr->line);
            }
            break;
        }

        case EXPR_CALL: {
            compile_expr(c, expr->call.callee);
            for (u32 i = 0; i < expr->call.arg_count; i++) {
                compile_expr(c, expr->call.args[i]);
            }
            emit_byte(c, (u8)OP_CALL, expr->line);
            emit_byte(c, (u8)expr->call.arg_count, expr->line);
            break;
        }

        case EXPR_BLOCK: {
            begin_scope(c);
            for (u32 i = 0; i < expr->block.stmt_count; i++) {
                compile_stmt(c, expr->block.stmts[i]);
            }
            if (expr->block.result_expr) {
                compile_expr(c, expr->block.result_expr);
            } else {
                emit_byte(c, (u8)OP_NIL, expr->line);
            }
            end_scope(c, expr->line);
            break;
        }

        case EXPR_SPAWN: {
            /* Structured Concurrency: Spawn Green Thread Fiber */
            if (expr->spawn.call_expr->kind == EXPR_CALL) {
                AstExpr *call = expr->spawn.call_expr;
                compile_expr(c, call->call.callee);
                for (u32 i = 0; i < call->call.arg_count; i++) {
                    compile_expr(c, call->call.args[i]);
                }
                emit_byte(c, (u8)OP_SPAWN, expr->line);
                emit_byte(c, (u8)call->call.arg_count, expr->line);
            } else {
                compile_expr(c, expr->spawn.call_expr);
                emit_byte(c, (u8)OP_SPAWN, expr->line);
                emit_byte(c, 0, expr->line);
            }
            break;
        }

        case EXPR_YIELD: {
            if (expr->yield.value) {
                compile_expr(c, expr->yield.value);
            } else {
                emit_byte(c, (u8)OP_NIL, expr->line);
            }
            emit_byte(c, (u8)OP_YIELD, expr->line);
            break;
        }

        case EXPR_CAP_REQ: {
            ObjString *cap_str = obj_string_create(c->arena, expr->cap_req.cap_name, expr->cap_req.cap_len);
            u16 cap_idx = chunk_add_constant(c->chunk, val_str(cap_str));
            emit_byte(c, (u8)OP_CAP_REQ, expr->line);
            emit_byte(c, (u8)(cap_idx & 0xFF), expr->line);
            emit_byte(c, (u8)((cap_idx >> 8) & 0xFF), expr->line);
            break;
        }

        default:
            break;
    }
}

static void compile_stmt(Compiler *c, AstStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            compile_expr(c, stmt->expr);
            emit_byte(c, (u8)OP_POP, stmt->line);
            break;

        case STMT_VAR: {
            if (stmt->var_decl.init) {
                compile_expr(c, stmt->var_decl.init);
            } else {
                emit_byte(c, (u8)OP_NIL, stmt->line);
            }

            if (c->scope_depth > 0) {
                add_local(c, stmt->var_decl.name, stmt->var_decl.name_len, stmt->var_decl.is_mut);
            } else {
                ObjString *name_str = obj_string_create(c->arena, stmt->var_decl.name, stmt->var_decl.name_len);
                u16 name_idx = chunk_add_constant(c->chunk, val_str(name_str));
                emit_byte(c, (u8)OP_DEFINE_GLOBAL, stmt->line);
                emit_byte(c, (u8)(name_idx & 0xFF), stmt->line);
                emit_byte(c, (u8)((name_idx >> 8) & 0xFF), stmt->line);
            }
            break;
        }

        case STMT_IF: {
            compile_expr(c, stmt->if_stmt.condition);
            usize then_jump = emit_jump(c, (u8)OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(c, (u8)OP_POP, stmt->line); /* Pop condition */

            compile_stmt(c, stmt->if_stmt.then_branch);

            usize else_jump = emit_jump(c, (u8)OP_JUMP, stmt->line);
            patch_jump(c, then_jump);
            emit_byte(c, (u8)OP_POP, stmt->line); /* Pop condition if false */

            if (stmt->if_stmt.else_branch) {
                compile_stmt(c, stmt->if_stmt.else_branch);
            }
            patch_jump(c, else_jump);
            break;
        }

        case STMT_WHILE: {
            LoopContext loop;
            loop.loop_start   = c->chunk->count;
            loop.break_count  = 0;
            loop.scope_depth  = c->scope_depth;
            loop.enclosing    = c->current_loop;
            c->current_loop   = &loop;

            compile_expr(c, stmt->while_stmt.condition);
            usize exit_jump = emit_jump(c, (u8)OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(c, (u8)OP_POP, stmt->line);

            compile_stmt(c, stmt->while_stmt.body);
            emit_loop(c, loop.loop_start, stmt->line);

            patch_jump(c, exit_jump);
            emit_byte(c, (u8)OP_POP, stmt->line);

            for (u32 i = 0; i < loop.break_count; i++) {
                patch_jump(c, loop.break_jumps[i]);
            }
            c->current_loop = loop.enclosing;
            break;
        }

        case STMT_LOOP: {
            LoopContext loop;
            loop.loop_start   = c->chunk->count;
            loop.break_count  = 0;
            loop.scope_depth  = c->scope_depth;
            loop.enclosing    = c->current_loop;
            c->current_loop   = &loop;

            compile_stmt(c, stmt->loop_stmt.body);
            emit_loop(c, loop.loop_start, stmt->line);

            for (u32 i = 0; i < loop.break_count; i++) {
                patch_jump(c, loop.break_jumps[i]);
            }
            c->current_loop = loop.enclosing;
            break;
        }

        case STMT_PRINT: {
            for (u32 i = 0; i < stmt->print_stmt.count; i++) {
                compile_expr(c, stmt->print_stmt.exprs[i]);
            }
            emit_byte(c, (u8)OP_PRINT, stmt->line);
            emit_byte(c, (u8)stmt->print_stmt.count, stmt->line);
            break;
        }

        case STMT_RETURN: {
            if (stmt->return_stmt.value) {
                compile_expr(c, stmt->return_stmt.value);
            } else {
                emit_byte(c, (u8)OP_NIL, stmt->line);
            }
            emit_byte(c, (u8)OP_RETURN, stmt->line);
            break;
        }

        case STMT_BREAK: {
            if (!c->current_loop) {
                rdx_print("[ERROR] 'break' used outside of a loop\n");
                c->had_error = TRUE;
                return;
            }
            usize jump = emit_jump(c, (u8)OP_JUMP, stmt->line);
            if (c->current_loop->break_count < MAX_BREAKS) {
                c->current_loop->break_jumps[c->current_loop->break_count++] = jump;
            }
            break;
        }

        case STMT_CONTINUE: {
            if (!c->current_loop) {
                rdx_print("[ERROR] 'continue' used outside of a loop\n");
                c->had_error = TRUE;
                return;
            }
            emit_loop(c, c->current_loop->loop_start, stmt->line);
            break;
        }
    }
}

static void compile_decl(Compiler *c, AstDecl *decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_STMT:
            compile_stmt(c, decl->stmt);
            break;

        case DECL_FUNCTION: {
            /* Create new function object */
            ObjFunction *fn = (ObjFunction*)arena_alloc_zero(c->arena, sizeof(ObjFunction));
            fn->name     = arena_strndup(c->arena, decl->fn_decl.name, decl->fn_decl.name_len);
            fn->name_len = decl->fn_decl.name_len;
            fn->arity    = decl->fn_decl.param_count;
            fn->chunk    = (Chunk*)arena_alloc(c->arena, sizeof(Chunk));
            chunk_init(fn->chunk, c->arena);

            Compiler fn_compiler;
            compiler_init(&fn_compiler, c, fn, fn->chunk, c->arena);
            fn_compiler.scope_depth = 1;

            /* Add parameters as local variables */
            for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
                add_local(&fn_compiler, decl->fn_decl.params[i].name, decl->fn_decl.params[i].name_len, FALSE);
            }

            compile_expr(&fn_compiler, decl->fn_decl.body);

            /* Implicit return nil if not returned */
            emit_byte(&fn_compiler, (u8)OP_NIL, decl->line);
            emit_byte(&fn_compiler, (u8)OP_RETURN, decl->line);

            /* Store function into global constant table */
            ObjString *name_str = obj_string_create(c->arena, decl->fn_decl.name, decl->fn_decl.name_len);
            u16 name_idx = chunk_add_constant(c->chunk, val_str(name_str));

            emit_constant(c, val_fn(fn), decl->line);
            emit_byte(c, (u8)OP_DEFINE_GLOBAL, decl->line);
            emit_byte(c, (u8)(name_idx & 0xFF), decl->line);
            emit_byte(c, (u8)((name_idx >> 8) & 0xFF), decl->line);
            break;
        }

        case DECL_SHAPE:
            /* Shapes currently define layout shapes for generational objects */
            break;
    }
}

ObjFunction *compiler_compile_program(AstProgram *program, Arena *arena) {
    if (!program) return NULL;

    ObjFunction *main_fn = (ObjFunction*)arena_alloc_zero(arena, sizeof(ObjFunction));
    main_fn->name     = "<main>";
    main_fn->name_len = 6;
    main_fn->arity    = 0;
    main_fn->chunk    = (Chunk*)arena_alloc(arena, sizeof(Chunk));
    chunk_init(main_fn->chunk, arena);

    Compiler compiler;
    compiler_init(&compiler, NULL, main_fn, main_fn->chunk, arena);

    for (u32 i = 0; i < program->decl_count; i++) {
        compile_decl(&compiler, program->decls[i]);
    }

    emit_byte(&compiler, (u8)OP_NIL, 1);
    emit_byte(&compiler, (u8)OP_HALT, 1);

    if (compiler.had_error) return NULL;
    return main_fn;
}
