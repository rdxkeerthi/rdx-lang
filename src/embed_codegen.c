/*
 * ============================================================================
 *  embed_codegen.c  —  RDX Freestanding MISRA-C Code Generator Implementation
 * ============================================================================
 */

#include "embed_codegen.h"
#include "comptime.h"

static void emit_str(EmbedCodeGen *gen, const char *s) {
    if (!s) return;
    usize len = my_strlen(s);
    while (gen->count + len + 1 >= gen->capacity) {
        usize new_cap = gen->capacity * 2;
        char *new_buf = (char*)arena_alloc(gen->arena, new_cap);
        my_memcpy(new_buf, gen->buffer, gen->count);
        gen->buffer   = new_buf;
        gen->capacity = new_cap;
    }
    my_memcpy(gen->buffer + gen->count, s, len);
    gen->count += len;
    gen->buffer[gen->count] = '\0';
}

static void emit_num(EmbedCodeGen *gen, i64 num) {
    char buf[32];
    if (num == 0) {
        emit_str(gen, "0");
        return;
    }
    bool neg = FALSE;
    if (num < 0) {
        neg = TRUE;
        num = -num;
    }
    int idx = 0;
    while (num > 0) {
        buf[idx++] = (char)('0' + (num % 10));
        num /= 10;
    }
    if (neg) buf[idx++] = '-';
    for (int i = 0; i < idx / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = tmp;
    }
    buf[idx] = '\0';
    emit_str(gen, buf);
}

static void emit_hex(EmbedCodeGen *gen, u64 hex_val) {
    emit_str(gen, "0x");
    char hex_digits[] = "0123456789ABCDEF";
    char buf[32];
    if (hex_val == 0) {
        emit_str(gen, "00UL");
        return;
    }
    int idx = 0;
    while (hex_val > 0) {
        buf[idx++] = hex_digits[hex_val & 0xF];
        hex_val >>= 4;
    }
    for (int i = 0; i < idx / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[idx - 1 - i];
        buf[idx - 1 - i] = tmp;
    }
    buf[idx] = '\0';
    emit_str(gen, buf);
    emit_str(gen, "UL");
}

static void emit_indent(EmbedCodeGen *gen) {
    for (u32 i = 0; i < gen->indent_level; i++) {
        emit_str(gen, "    ");
    }
}

static const char *map_c_type(const char *type_name) {
    if (!type_name) return "void";
    if (my_strcmp(type_name, "u8") == 0) return "u8";
    if (my_strcmp(type_name, "u16") == 0) return "u16";
    if (my_strcmp(type_name, "u32") == 0) return "u32";
    if (my_strcmp(type_name, "u64") == 0) return "u64";
    if (my_strcmp(type_name, "i8") == 0) return "i8";
    if (my_strcmp(type_name, "i16") == 0) return "i16";
    if (my_strcmp(type_name, "i32") == 0 || my_strcmp(type_name, "int") == 0) return "i32";
    if (my_strcmp(type_name, "i64") == 0) return "i64";
    if (my_strcmp(type_name, "usize") == 0) return "usize";
    if (my_strcmp(type_name, "isize") == 0) return "isize";
    if (my_strcmp(type_name, "bool") == 0) return "bool";
    if (my_strcmp(type_name, "f32") == 0) return "f32";
    if (my_strcmp(type_name, "f64") == 0 || my_strcmp(type_name, "float") == 0) return "f64";
    if (my_strcmp(type_name, "void") == 0) return "void";
    return type_name;
}

void embed_codegen_init(EmbedCodeGen *gen, Arena *arena) {
    gen->arena        = arena;
    gen->count        = 0;
    gen->capacity     = 16 * 1024;
    gen->indent_level = 0;
    gen->had_error    = FALSE;
    gen->buffer       = (char*)arena_alloc(arena, gen->capacity);
    gen->buffer[0]    = '\0';
}

static void gen_expr(EmbedCodeGen *gen, AstExpr *expr);
static void gen_stmt(EmbedCodeGen *gen, AstStmt *stmt);

static void gen_expr(EmbedCodeGen *gen, AstExpr *expr) {
    if (!expr) return;

    switch (expr->kind) {
        case EXPR_INT:
            emit_num(gen, expr->int_val);
            break;

        case EXPR_FLOAT:
            emit_num(gen, (i64)expr->float_val);
            emit_str(gen, ".");
            emit_num(gen, (i64)((expr->float_val - (i64)expr->float_val) * 10000.0));
            emit_str(gen, "f");
            break;

        case EXPR_STR:
            emit_str(gen, "\"");
            emit_str(gen, expr->str_val.chars);
            emit_str(gen, "\"");
            break;

        case EXPR_BOOL:
            emit_str(gen, expr->bool_val ? "true" : "false");
            break;

        case EXPR_IDENT:
            emit_str(gen, expr->ident.name);
            break;

        case EXPR_UNARY:
            switch (expr->unary.op) {
                case TOK_MINUS: emit_str(gen, "-("); break;
                case TOK_NOT:   emit_str(gen, "!("); break;
                case TOK_TILDE: emit_str(gen, "~("); break;
                default: emit_str(gen, "("); break;
            }
            gen_expr(gen, expr->unary.operand);
            emit_str(gen, ")");
            break;

        case EXPR_BINARY:
            emit_str(gen, "(");
            gen_expr(gen, expr->binary.left);
            emit_str(gen, " ");
            switch (expr->binary.op) {
                case TOK_PLUS:    emit_str(gen, "+"); break;
                case TOK_MINUS:   emit_str(gen, "-"); break;
                case TOK_STAR:    emit_str(gen, "*"); break;
                case TOK_SLASH:   emit_str(gen, "/"); break;
                case TOK_PERCENT: emit_str(gen, "%"); break;
                case TOK_AMP:     emit_str(gen, "&"); break;
                case TOK_PIPE:    emit_str(gen, "|"); break;
                case TOK_CARET:   emit_str(gen, "^"); break;
                case TOK_SHL:     emit_str(gen, "<<"); break;
                case TOK_SHR:     emit_str(gen, ">>"); break;
                case TOK_EQ:      emit_str(gen, "=="); break;
                case TOK_NEQ:     emit_str(gen, "!="); break;
                case TOK_LT:      emit_str(gen, "<"); break;
                case TOK_LTE:     emit_str(gen, "<="); break;
                case TOK_GT:      emit_str(gen, ">"); break;
                case TOK_GTE:     emit_str(gen, ">="); break;
                case TOK_AND:     emit_str(gen, "&&"); break;
                case TOK_OR:      emit_str(gen, "||"); break;
                default: emit_str(gen, "?"); break;
            }
            emit_str(gen, " ");
            gen_expr(gen, expr->binary.right);
            emit_str(gen, ")");
            break;

        case EXPR_ASSIGN:
            gen_expr(gen, expr->assign.target);
            switch (expr->assign.op) {
                case TOK_ASSIGN:       emit_str(gen, " = "); break;
                case TOK_PLUS_ASSIGN:  emit_str(gen, " += "); break;
                case TOK_MINUS_ASSIGN: emit_str(gen, " -= "); break;
                case TOK_STAR_ASSIGN:  emit_str(gen, " *= "); break;
                case TOK_SLASH_ASSIGN: emit_str(gen, " /= "); break;
                default: emit_str(gen, " = "); break;
            }
            gen_expr(gen, expr->assign.value);
            break;

        case EXPR_CALL:
            gen_expr(gen, expr->call.callee);
            emit_str(gen, "(");
            for (u32 i = 0; i < expr->call.arg_count; i++) {
                gen_expr(gen, expr->call.args[i]);
                if (i + 1 < expr->call.arg_count) emit_str(gen, ", ");
            }
            emit_str(gen, ")");
            break;

        case EXPR_MEMBER:
            if (expr->member.target->kind == EXPR_IDENT) {
                /* Peripheral register access mapping */
                emit_str(gen, expr->member.target->ident.name);
                emit_str(gen, "_");
                emit_str(gen, expr->member.field);
            } else {
                gen_expr(gen, expr->member.target);
                emit_str(gen, ".");
                emit_str(gen, expr->member.field);
            }
            break;

        case EXPR_INDEX:
            gen_expr(gen, expr->index.target);
            emit_str(gen, "[");
            gen_expr(gen, expr->index.index);
            emit_str(gen, "]");
            break;

        case EXPR_BIT_RANGE: {
            /* Bit-range read: ((target >> lo) & ((1ULL << (hi - lo + 1)) - 1)) */
            emit_str(gen, "(((");
            gen_expr(gen, expr->bit_range.target);
            emit_str(gen, ") >> (");
            gen_expr(gen, expr->bit_range.low_bit);
            emit_str(gen, ")) & ((1ULL << ((");
            gen_expr(gen, expr->bit_range.high_bit);
            emit_str(gen, ") - (");
            gen_expr(gen, expr->bit_range.low_bit);
            emit_str(gen, ") + 1ULL)) - 1ULL))");
            break;
        }

        case EXPR_COMPTIME: {
            /* Evaluate compile-time constant */
            ComptimeContext comptime;
            comptime_init(&comptime, gen->arena);
            i64 evaluated = 0;
            if (comptime_eval_int(&comptime, expr->comptime_expr.expr, &evaluated)) {
                emit_num(gen, evaluated);
            } else {
                gen_expr(gen, expr->comptime_expr.expr);
            }
            break;
        }

        case EXPR_ASM:
            emit_str(gen, "__asm__ ");
            if (expr->inline_asm.is_volatile) emit_str(gen, "volatile ");
            emit_str(gen, "(\"");
            emit_str(gen, expr->inline_asm.instruction);
            emit_str(gen, "\")");
            break;

        case EXPR_BLOCK:
            emit_str(gen, "{\n");
            gen->indent_level++;
            for (u32 i = 0; i < expr->block.stmt_count; i++) {
                gen_stmt(gen, expr->block.stmts[i]);
            }
            if (expr->block.result_expr) {
                emit_indent(gen);
                gen_expr(gen, expr->block.result_expr);
                emit_str(gen, ";\n");
            }
            gen->indent_level--;
            emit_indent(gen);
            emit_str(gen, "}");
            break;

        default:
            break;
    }
}

static void gen_stmt(EmbedCodeGen *gen, AstStmt *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            emit_indent(gen);
            gen_expr(gen, stmt->expr);
            emit_str(gen, ";\n");
            break;

        case STMT_VAR: {
            emit_indent(gen);
            const char *c_type = map_c_type(stmt->var_decl.type_name ? stmt->var_decl.type_name : "u32");
            emit_str(gen, c_type);
            emit_str(gen, " ");
            emit_str(gen, stmt->var_decl.name);
            if (stmt->var_decl.init) {
                emit_str(gen, " = ");
                gen_expr(gen, stmt->var_decl.init);
            }
            emit_str(gen, ";\n");
            break;
        }

        case STMT_IF:
            emit_indent(gen);
            emit_str(gen, "if (");
            gen_expr(gen, stmt->if_stmt.condition);
            emit_str(gen, ") ");
            if (stmt->if_stmt.then_branch->kind == STMT_EXPR && stmt->if_stmt.then_branch->expr->kind == EXPR_BLOCK) {
                gen_expr(gen, stmt->if_stmt.then_branch->expr);
            } else {
                emit_str(gen, "{\n");
                gen->indent_level++;
                gen_stmt(gen, stmt->if_stmt.then_branch);
                gen->indent_level--;
                emit_indent(gen);
                emit_str(gen, "}");
            }
            if (stmt->if_stmt.else_branch) {
                emit_str(gen, " else ");
                if (stmt->if_stmt.else_branch->kind == STMT_IF) {
                    gen_stmt(gen, stmt->if_stmt.else_branch);
                } else if (stmt->if_stmt.else_branch->kind == STMT_EXPR && stmt->if_stmt.else_branch->expr->kind == EXPR_BLOCK) {
                    gen_expr(gen, stmt->if_stmt.else_branch->expr);
                    emit_str(gen, "\n");
                }
            } else {
                emit_str(gen, "\n");
            }
            break;

        case STMT_WHILE:
            emit_indent(gen);
            emit_str(gen, "while (");
            gen_expr(gen, stmt->while_stmt.condition);
            emit_str(gen, ") ");
            if (stmt->while_stmt.body->kind == STMT_EXPR && stmt->while_stmt.body->expr->kind == EXPR_BLOCK) {
                gen_expr(gen, stmt->while_stmt.body->expr);
                emit_str(gen, "\n");
            } else {
                emit_str(gen, "{\n");
                gen->indent_level++;
                gen_stmt(gen, stmt->while_stmt.body);
                gen->indent_level--;
                emit_indent(gen);
                emit_str(gen, "}\n");
            }
            break;

        case STMT_LOOP:
            emit_indent(gen);
            emit_str(gen, "for (;;) {\n");
            gen->indent_level++;
            gen_stmt(gen, stmt->loop_stmt.body);
            gen->indent_level--;
            emit_indent(gen);
            emit_str(gen, "}\n");
            break;

        case STMT_RETURN:
            emit_indent(gen);
            emit_str(gen, "return");
            if (stmt->return_stmt.value) {
                emit_str(gen, " ");
                gen_expr(gen, stmt->return_stmt.value);
            }
            emit_str(gen, ";\n");
            break;

        case STMT_BREAK:
            emit_indent(gen);
            emit_str(gen, "break;\n");
            break;

        case STMT_CONTINUE:
            emit_indent(gen);
            emit_str(gen, "continue;\n");
            break;

        case STMT_ASM:
            emit_indent(gen);
            emit_str(gen, "__asm__ ");
            if (stmt->asm_stmt.is_volatile) emit_str(gen, "volatile ");
            emit_str(gen, "(\"");
            emit_str(gen, stmt->asm_stmt.instruction);
            emit_str(gen, "\");\n");
            break;

        default:
            break;
    }
}

static void gen_reg_block(EmbedCodeGen *gen, AstDecl *decl) {
    emit_str(gen, "/* Peripheral Register Block: ");
    emit_str(gen, decl->reg_block.name);
    emit_str(gen, " */\n");

    emit_str(gen, "#define ");
    emit_str(gen, decl->reg_block.name);
    emit_str(gen, "_BASE ");
    emit_hex(gen, decl->reg_block.base_address);
    emit_str(gen, "\n");

    for (u32 i = 0; i < decl->reg_block.reg_count; i++) {
        AstRegMember *reg = &decl->reg_block.registers[i];
        emit_str(gen, "#define ");
        emit_str(gen, decl->reg_block.name);
        emit_str(gen, "_");
        emit_str(gen, reg->name);
        emit_str(gen, " (*((volatile ");
        emit_str(gen, map_c_type(reg->type_name));
        emit_str(gen, "*)(");
        emit_str(gen, decl->reg_block.name);
        emit_str(gen, "_BASE + ");
        emit_hex(gen, reg->offset);
        emit_str(gen, ")))\n");
    }
    emit_str(gen, "\n");
}

static void gen_struct_decl(EmbedCodeGen *gen, AstDecl *decl) {
    if (decl->struct_decl.is_packed) {
        emit_str(gen, "typedef struct __attribute__((packed)) {\n");
    } else {
        emit_str(gen, "typedef struct {\n");
    }
    gen->indent_level++;
    for (u32 i = 0; i < decl->struct_decl.field_count; i++) {
        AstField *field = &decl->struct_decl.fields[i];
        emit_indent(gen);
        emit_str(gen, map_c_type(field->type_name));
        emit_str(gen, " ");
        emit_str(gen, field->name);
        emit_str(gen, ";\n");
    }
    gen->indent_level--;
    emit_str(gen, "} ");
    emit_str(gen, decl->struct_decl.name);
    emit_str(gen, ";\n\n");
}

static void gen_fn_decl(EmbedCodeGen *gen, AstDecl *decl) {
    const char *ret_type = map_c_type(decl->fn_decl.return_type ? decl->fn_decl.return_type : "void");
    emit_str(gen, ret_type);
    emit_str(gen, " ");
    emit_str(gen, decl->fn_decl.name);
    emit_str(gen, "(");

    if (decl->fn_decl.param_count == 0) {
        emit_str(gen, "void");
    } else {
        for (u32 i = 0; i < decl->fn_decl.param_count; i++) {
            AstParam *p = &decl->fn_decl.params[i];
            emit_str(gen, map_c_type(p->type_name ? p->type_name : "u32"));
            emit_str(gen, " ");
            emit_str(gen, p->name);
            if (i + 1 < decl->fn_decl.param_count) emit_str(gen, ", ");
        }
    }
    emit_str(gen, ") ");

    if (decl->fn_decl.body) {
        gen_expr(gen, decl->fn_decl.body);
    } else {
        emit_str(gen, "{}");
    }
    emit_str(gen, "\n\n");
}

static void gen_isr_decl(EmbedCodeGen *gen, AstDecl *decl) {
    emit_str(gen, "/* ISR Vector ");
    emit_num(gen, decl->isr_decl.vector_num);
    emit_str(gen, " */\n");
    emit_str(gen, "void __attribute__((interrupt)) ");
    emit_str(gen, decl->isr_decl.name);
    emit_str(gen, "(void) ");

    if (decl->isr_decl.body) {
        gen_expr(gen, decl->isr_decl.body);
    } else {
        emit_str(gen, "{}");
    }
    emit_str(gen, "\n\n");
}

const char *embed_codegen_generate(EmbedCodeGen *gen, AstProgram *prog) {
    if (!prog) return NULL;

    /* Emit standard freestanding embedded types */
    emit_str(gen, "/* ========================================================================= */\n");
    emit_str(gen, "/*  Generated by rdx-lang Freestanding Embedded MISRA-C Backend              */\n");
    emit_str(gen, "/*  Target: Bare-Metal ARM Cortex-M / RISC-V / ESP32 (no_std)                */\n");
    emit_str(gen, "/* ========================================================================= */\n\n");

    emit_str(gen, "#ifndef RDX_FREESTANDING_H\n");
    emit_str(gen, "#define RDX_FREESTANDING_H\n\n");
    emit_str(gen, "typedef unsigned char       u8;\n");
    emit_str(gen, "typedef unsigned short      u16;\n");
    emit_str(gen, "typedef unsigned int        u32;\n");
    emit_str(gen, "typedef unsigned long long  u64;\n");
    emit_str(gen, "typedef signed char         i8;\n");
    emit_str(gen, "typedef signed short        i16;\n");
    emit_str(gen, "typedef signed int          i32;\n");
    emit_str(gen, "typedef signed long long    i64;\n");
    emit_str(gen, "typedef unsigned long long  usize;\n");
    emit_str(gen, "typedef signed long long    isize;\n");
    emit_str(gen, "typedef float               f32;\n");
    emit_str(gen, "typedef double              f64;\n");
    emit_str(gen, "#ifndef __cplusplus\n");
    emit_str(gen, "#ifndef bool\n");
    emit_str(gen, "#define bool int\n");
    emit_str(gen, "#define true 1\n");
    emit_str(gen, "#define false 0\n");
    emit_str(gen, "#endif\n");
    emit_str(gen, "#endif\n\n");

    /* First pass: Peripherals and Structs */
    for (u32 i = 0; i < prog->decl_count; i++) {
        AstDecl *d = prog->decls[i];
        if (d->kind == DECL_REG_BLOCK) {
            gen_reg_block(gen, d);
        } else if (d->kind == DECL_STRUCT) {
            gen_struct_decl(gen, d);
        }
    }

    /* Second pass: Functions, ISRs, Statements */
    for (u32 i = 0; i < prog->decl_count; i++) {
        AstDecl *d = prog->decls[i];
        if (d->kind == DECL_FUNCTION) {
            gen_fn_decl(gen, d);
        } else if (d->kind == DECL_ISR) {
            gen_isr_decl(gen, d);
        } else if (d->kind == DECL_STMT) {
            gen_stmt(gen, d->stmt);
        }
    }

    emit_str(gen, "#endif /* RDX_FREESTANDING_H */\n");
    return gen->buffer;
}
