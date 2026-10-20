/*
 * ============================================================================
 *  sem_embed.c  —  RDX Embedded Semantic Analyzer Implementation
 * ============================================================================
 */

#include "sem_embed.h"
#include "comptime.h"

static void sem_error(EmbeddedAnalyzer *sem, u32 line, u32 col, const char *msg) {
    sem->had_error = TRUE;
    sem->error_count++;
    rdx_print("[EMBEDDED SEMANTIC ERROR] Line ");
    rdx_print_num(line);
    rdx_print(" Col ");
    rdx_print_num(col);
    rdx_print(": ");
    rdx_print(msg);
    rdx_print("\n");
}

void sem_embed_init(EmbeddedAnalyzer *sem, Arena *arena) {
    sem->arena             = arena;
    sem->peripheral_count  = 0;
    sem->granted_cap_count = 0;
    sem->had_error         = FALSE;
    sem->current_fn_name   = NULL;
    sem->error_count       = 0;
}

static PeripheralSymbol *find_peripheral(EmbeddedAnalyzer *sem, const char *name, usize length) {
    for (u32 i = 0; i < sem->peripheral_count; i++) {
        if (sem->peripherals[i].name_len == length &&
            my_strncmp(sem->peripherals[i].name, name, length) == 0) {
            return &sem->peripherals[i];
        }
    }
    return NULL;
}

static bool has_capability(const EmbeddedAnalyzer *sem, const char *cap_name, usize cap_len) {
    if (!cap_name || cap_len == 0) return TRUE;
    for (u32 i = 0; i < sem->granted_cap_count; i++) {
        if (my_strncmp(sem->granted_caps[i], cap_name, cap_len) == 0 &&
            my_strlen(sem->granted_caps[i]) == cap_len) {
            return TRUE;
        }
    }
    return FALSE;
}

bool sem_embed_check_peripheral_access(EmbeddedAnalyzer *sem, const char *periph_name, usize periph_len, u32 line, u32 col) {
    PeripheralSymbol *p = find_peripheral(sem, periph_name, periph_len);
    if (!p) return TRUE; /* Standard variable/symbol */

    if (p->cap_token && p->cap_len > 0) {
        if (!has_capability(sem, p->cap_token, p->cap_len)) {
            rdx_print("[CAPABILITY VIOLATION] Function '");
            if (sem->current_fn_name) rdx_print(sem->current_fn_name);
            else rdx_print("<global>");
            rdx_print("' attempted to access peripheral '");
            rdx_print(p->name);
            rdx_print("' without possessing required capability '");
            rdx_print(p->cap_token);
            rdx_print("'\n");
            sem_error(sem, line, col, "Hardware peripheral access rejected due to missing capability token");
            return FALSE;
        }
    }
    return TRUE;
}

static void analyze_expr(EmbeddedAnalyzer *sem, AstExpr *expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_MEMBER: {
            if (expr->member.target->kind == EXPR_IDENT) {
                const char *target_name = expr->member.target->ident.name;
                usize target_len = expr->member.target->ident.length;
                sem_embed_check_peripheral_access(sem, target_name, target_len, expr->line, expr->col);
            }
            analyze_expr(sem, expr->member.target);
            break;
        }

        case EXPR_BIT_RANGE: {
            analyze_expr(sem, expr->bit_range.target);
            analyze_expr(sem, expr->bit_range.high_bit);
            analyze_expr(sem, expr->bit_range.low_bit);

            ComptimeContext comptime;
            comptime_init(&comptime, sem->arena);
            i64 hi = 0, lo = 0;
            if (comptime_eval_int(&comptime, expr->bit_range.high_bit, &hi) &&
                comptime_eval_int(&comptime, expr->bit_range.low_bit, &lo)) {
                if (hi < lo) {
                    sem_error(sem, expr->line, expr->col, "Bit range high index must be >= low index");
                }
                if (hi >= 64 || lo < 0) {
                    sem_error(sem, expr->line, expr->col, "Bit range index out of bounds (0..63)");
                }
            }
            break;
        }

        case EXPR_BINARY:
            analyze_expr(sem, expr->binary.left);
            analyze_expr(sem, expr->binary.right);
            break;

        case EXPR_UNARY:
            analyze_expr(sem, expr->unary.operand);
            break;

        case EXPR_ASSIGN:
            analyze_expr(sem, expr->assign.target);
            analyze_expr(sem, expr->assign.value);
            break;

        case EXPR_CALL:
            analyze_expr(sem, expr->call.callee);
            for (u32 i = 0; i < expr->call.arg_count; i++) {
                analyze_expr(sem, expr->call.args[i]);
            }
            break;

        case EXPR_INDEX:
            analyze_expr(sem, expr->index.target);
            analyze_expr(sem, expr->index.index);
            break;

        case EXPR_BLOCK:
            for (u32 i = 0; i < expr->block.stmt_count; i++) {
                AstStmt *s = expr->block.stmts[i];
                if (s && s->kind == STMT_EXPR) analyze_expr(sem, s->expr);
                else if (s && s->kind == STMT_VAR && s->var_decl.init) analyze_expr(sem, s->var_decl.init);
            }
            if (expr->block.result_expr) analyze_expr(sem, expr->block.result_expr);
            break;

        default:
            break;
    }
}

static void analyze_stmt(EmbeddedAnalyzer *sem, AstStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            analyze_expr(sem, stmt->expr);
            break;

        case STMT_VAR:
            if (stmt->var_decl.init) {
                analyze_expr(sem, stmt->var_decl.init);
            }
            break;

        case STMT_IF:
            analyze_expr(sem, stmt->if_stmt.condition);
            analyze_stmt(sem, stmt->if_stmt.then_branch);
            if (stmt->if_stmt.else_branch) {
                analyze_stmt(sem, stmt->if_stmt.else_branch);
            }
            break;

        case STMT_WHILE:
            analyze_expr(sem, stmt->while_stmt.condition);
            analyze_stmt(sem, stmt->while_stmt.body);
            break;

        case STMT_LOOP:
            analyze_stmt(sem, stmt->loop_stmt.body);
            break;

        case STMT_RETURN:
            if (stmt->return_stmt.value) {
                analyze_expr(sem, stmt->return_stmt.value);
            }
            break;

        default:
            break;
    }
}

static void analyze_decl(EmbeddedAnalyzer *sem, AstDecl *decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_REG_BLOCK: {
            if (sem->peripheral_count >= MAX_PERIPHERALS) {
                sem_error(sem, decl->line, decl->col, "Too many peripheral register blocks defined");
                return;
            }
            PeripheralSymbol *p = &sem->peripherals[sem->peripheral_count++];
            p->name         = decl->reg_block.name;
            p->name_len     = decl->reg_block.name_len;
            p->base_address = decl->reg_block.base_address;
            p->cap_token    = decl->reg_block.cap_token;
            p->cap_len      = decl->reg_block.cap_len;
            p->registers    = decl->reg_block.registers;
            p->reg_count    = decl->reg_block.reg_count;
            break;
        }

        case DECL_FUNCTION: {
            sem->current_fn_name   = decl->fn_decl.name;
            sem->granted_cap_count = 0;

            /* Check function parameter capability tokens */
            for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
                AstParam *param = &decl->fn_decl.params[i];
                if (param->type_name) {
                    /* If param type specifies cap or capability token */
                    if (sem->granted_cap_count < MAX_CAPABILITIES) {
                        sem->granted_caps[sem->granted_cap_count++] = param->type_name;
                    }
                }
            }

            if (decl->fn_decl.body) {
                analyze_expr(sem, decl->fn_decl.body);
            }
            sem->current_fn_name = NULL;
            break;
        }

        case DECL_ISR: {
            sem->current_fn_name = decl->isr_decl.name;
            if (decl->isr_decl.vector_num < 0 || decl->isr_decl.vector_num > 255) {
                sem_error(sem, decl->line, decl->col, "Invalid ISR vector number (must be 0..255)");
            }
            if (decl->isr_decl.body) {
                analyze_expr(sem, decl->isr_decl.body);
            }
            sem->current_fn_name = NULL;
            break;
        }

        case DECL_STMT:
            analyze_stmt(sem, decl->stmt);
            break;

        default:
            break;
    }
}

bool sem_embed_analyze_program(EmbeddedAnalyzer *sem, AstProgram *prog) {
    if (!prog) return FALSE;

    /* First pass: register all peripherals and structs */
    for (u32 i = 0; i < prog->decl_count; i++) {
        if (prog->decls[i]->kind == DECL_REG_BLOCK) {
            analyze_decl(sem, prog->decls[i]);
        }
    }

    /* Second pass: analyze functions, ISRs, and statements */
    for (u32 i = 0; i < prog->decl_count; i++) {
        if (prog->decls[i]->kind != DECL_REG_BLOCK) {
            analyze_decl(sem, prog->decls[i]);
        }
    }

    return !sem->had_error;
}
