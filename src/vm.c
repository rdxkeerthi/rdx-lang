/*
 * ============================================================================
 *  vm.c  —  RDX Stack-Based Bytecode Virtual Machine Implementation
 * ============================================================================
 */

#include "vm.h"
#include "parser.h"
#include "compiler.h"

void vm_init(VM *vm, u64 capabilities) {
    arena_init(&vm->vm_arena, 64 * 1024);
    gen_table_init(&vm->gen_table, &vm->vm_arena, 1024);
    scheduler_init(&vm->sched, &vm->vm_arena);
    vm->global_count         = 0;
    vm->default_capabilities = capabilities;
}

void vm_free(VM *vm) {
    while (scheduler_has_work(&vm->sched)) {
        scheduler_finish_current(&vm->sched);
    }
    arena_free(&vm->vm_arena);
}

void vm_set_global(VM *vm, const char *name, usize length, RdxValue value) {
    for (u32 i = 0; i < vm->global_count; i++) {
        if (vm->globals[i].name_len == length && my_strncmp(vm->globals[i].name, name, length) == 0) {
            vm->globals[i].value = value;
            return;
        }
    }
    if (vm->global_count < VM_MAX_GLOBALS) {
        vm->globals[vm->global_count].name     = arena_strndup(&vm->vm_arena, name, length);
        vm->globals[vm->global_count].name_len = length;
        vm->globals[vm->global_count].value    = value;
        vm->global_count++;
    }
}

bool vm_get_global(const VM *vm, const char *name, usize length, RdxValue *out) {
    for (u32 i = 0; i < vm->global_count; i++) {
        if (vm->globals[i].name_len == length && my_strncmp(vm->globals[i].name, name, length) == 0) {
            *out = vm->globals[i].value;
            return TRUE;
        }
    }
    return FALSE;
}

static inline u16 read_u16(CallFrame *frame) {
    frame->ip += 2;
    return (u16)(frame->ip[-2] | (frame->ip[-1] << 8));
}

#define READ_BYTE(frame) (*frame->ip++)
#define READ_CONSTANT(frame, chunk) (chunk->constants.values[read_u16(frame)])

InterpretResult vm_run_function(VM *vm, ObjFunction *fn) {
    if (!fn) return INTERPRET_COMPILE_ERROR;

    /* Spawn main fiber */
    RdxFiber *main_fiber = scheduler_spawn(&vm->sched, fn, vm->default_capabilities);
    if (!main_fiber) return INTERPRET_RUNTIME_ERROR;

    /* Scheduler Loop */
    while (scheduler_has_work(&vm->sched)) {
        RdxFiber *fiber = scheduler_next_ready(&vm->sched);
        if (!fiber) break;

        /* Execute time-slice or until yield/completion */
        u32 instruction_budget = 1000;

        while (instruction_budget-- > 0 && fiber->state == FIBER_RUNNING) {
            CallFrame *frame = &fiber->frames[fiber->frame_count - 1];
            Chunk     *chunk = frame->function->chunk;

            u8 instruction = READ_BYTE(frame);

            switch (instruction) {
                case OP_CONSTANT: {
                    RdxValue constant = READ_CONSTANT(frame, chunk);
                    fiber_push(fiber, constant);
                    break;
                }

                case OP_NIL:   fiber_push(fiber, val_nil()); break;
                case OP_TRUE:  fiber_push(fiber, val_bool(TRUE)); break;
                case OP_FALSE: fiber_push(fiber, val_bool(FALSE)); break;

                case OP_POP:
                    fiber_pop(fiber);
                    break;

                case OP_DUP: {
                    RdxValue top = fiber_peek(fiber, 0);
                    fiber_push(fiber, top);
                    break;
                }

                case OP_GET_LOCAL: {
                    u16 slot = read_u16(frame);
                    fiber_push(fiber, frame->slots[slot]);
                    break;
                }

                case OP_SET_LOCAL: {
                    u16 slot = read_u16(frame);
                    frame->slots[slot] = fiber_peek(fiber, 0);
                    break;
                }

                case OP_DEFINE_GLOBAL: {
                    u16 name_idx = read_u16(frame);
                    RdxValue name_val = chunk->constants.values[name_idx];
                    RdxValue val = fiber_pop(fiber);
                    if (name_val.type == VAL_STR && name_val.as.str_val) {
                        vm_set_global(vm, name_val.as.str_val->chars, name_val.as.str_val->length, val);
                    }
                    break;
                }

                case OP_GET_GLOBAL: {
                    u16 name_idx = read_u16(frame);
                    RdxValue name_val = chunk->constants.values[name_idx];
                    if (name_val.type == VAL_STR && name_val.as.str_val) {
                        RdxValue val;
                        if (!vm_get_global(vm, name_val.as.str_val->chars, name_val.as.str_val->length, &val)) {
                            rdx_print("[ERROR] Undefined global variable '");
                            rdx_print(name_val.as.str_val->chars);
                            rdx_print("'\n");
                            fiber->state = FIBER_ERROR;
                            return INTERPRET_RUNTIME_ERROR;
                        }
                        fiber_push(fiber, val);
                    }
                    break;
                }

                case OP_SET_GLOBAL: {
                    u16 name_idx = read_u16(frame);
                    RdxValue name_val = chunk->constants.values[name_idx];
                    if (name_val.type == VAL_STR && name_val.as.str_val) {
                        vm_set_global(vm, name_val.as.str_val->chars, name_val.as.str_val->length, fiber_peek(fiber, 0));
                    }
                    break;
                }

                case OP_ADD: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_int(a.as.int_val + b.as.int_val));
                    } else if (a.type == VAL_FLOAT || b.type == VAL_FLOAT) {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_float(fa + fb));
                    } else if (a.type == VAL_STR && b.type == VAL_STR) {
                        /* String concatenation */
                        usize total_len = a.as.str_val->length + b.as.str_val->length;
                        ObjString *res = (ObjString*)arena_alloc(&fiber->task_arena, sizeof(ObjString) + total_len);
                        res->length = total_len;
                        my_memcpy(res->chars, a.as.str_val->chars, a.as.str_val->length);
                        my_memcpy(res->chars + a.as.str_val->length, b.as.str_val->chars, b.as.str_val->length);
                        res->chars[total_len] = '\0';
                        fiber_push(fiber, val_str(res));
                    }
                    break;
                }

                case OP_SUB: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_int(a.as.int_val - b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_float(fa - fb));
                    }
                    break;
                }

                case OP_MUL: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_int(a.as.int_val * b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_float(fa * fb));
                    }
                    break;
                }

                case OP_DIV: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (b.type == VAL_INT && b.as.int_val == 0) {
                        rdx_print("[ERROR] Division by zero\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_int(a.as.int_val / b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_float(fa / fb));
                    }
                    break;
                }

                case OP_MOD: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (b.as.int_val == 0) {
                        rdx_print("[ERROR] Modulo by zero\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    fiber_push(fiber, val_int(a.as.int_val % b.as.int_val));
                    break;
                }

                case OP_NEG: {
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT) fiber_push(fiber, val_int(-a.as.int_val));
                    else if (a.type == VAL_FLOAT) fiber_push(fiber, val_float(-a.as.float_val));
                    break;
                }

                case OP_NOT: {
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_bool(!value_is_truthy(a)));
                    break;
                }

                case OP_BIT_NOT: {
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(~a.as.int_val));
                    break;
                }

                case OP_BIT_AND: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(a.as.int_val & b.as.int_val));
                    break;
                }

                case OP_BIT_OR: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(a.as.int_val | b.as.int_val));
                    break;
                }

                case OP_BIT_XOR: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(a.as.int_val ^ b.as.int_val));
                    break;
                }

                case OP_SHL: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(a.as.int_val << b.as.int_val));
                    break;
                }

                case OP_SHR: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_int(a.as.int_val >> b.as.int_val));
                    break;
                }

                case OP_EQ: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_bool(value_equals(a, b)));
                    break;
                }

                case OP_NEQ: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    fiber_push(fiber, val_bool(!value_equals(a, b)));
                    break;
                }

                case OP_LT: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_bool(a.as.int_val < b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_bool(fa < fb));
                    }
                    break;
                }

                case OP_LTE: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_bool(a.as.int_val <= b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_bool(fa <= fb));
                    }
                    break;
                }

                case OP_GT: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_bool(a.as.int_val > b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_bool(fa > fb));
                    }
                    break;
                }

                case OP_GTE: {
                    RdxValue b = fiber_pop(fiber);
                    RdxValue a = fiber_pop(fiber);
                    if (a.type == VAL_INT && b.type == VAL_INT) {
                        fiber_push(fiber, val_bool(a.as.int_val >= b.as.int_val));
                    } else {
                        double fa = (a.type == VAL_FLOAT) ? a.as.float_val : (double)a.as.int_val;
                        double fb = (b.type == VAL_FLOAT) ? b.as.float_val : (double)b.as.int_val;
                        fiber_push(fiber, val_bool(fa >= fb));
                    }
                    break;
                }

                case OP_JUMP: {
                    u16 offset = read_u16(frame);
                    frame->ip += offset;
                    break;
                }

                case OP_JUMP_IF_FALSE: {
                    u16 offset = read_u16(frame);
                    if (!value_is_truthy(fiber_peek(fiber, 0))) {
                        frame->ip += offset;
                    }
                    break;
                }

                case OP_LOOP: {
                    u16 offset = read_u16(frame);
                    frame->ip -= offset;
                    break;
                }

                case OP_CALL: {
                    u8 arg_count = READ_BYTE(frame);
                    RdxValue callee = fiber_peek(fiber, arg_count);

                    if (callee.type != VAL_FN || !callee.as.fn_val) {
                        rdx_print("[ERROR] Can only call functions\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    ObjFunction *target_fn = callee.as.fn_val;
                    if (target_fn->arity != arg_count) {
                        rdx_print("[ERROR] Expected ");
                        rdx_print_num((i64)target_fn->arity);
                        rdx_print(" arguments but got ");
                        rdx_print_num((i64)arg_count);
                        rdx_print("\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    if (fiber->frame_count >= FIBER_FRAMES_MAX) {
                        rdx_print("[ERROR] Call frame stack overflow\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    CallFrame *new_frame  = &fiber->frames[fiber->frame_count++];
                    new_frame->function   = target_fn;
                    new_frame->ip         = target_fn->chunk->code;
                    new_frame->slots      = fiber->stack_top - arg_count - 1;
                    break;
                }

                case OP_RETURN: {
                    RdxValue result = fiber_pop(fiber);
                    fiber->frame_count--;

                    if (fiber->frame_count == 0) {
                        /* Fiber completed execution */
                        scheduler_finish_current(&vm->sched);
                        break;
                    }

                    fiber->stack_top = frame->slots;
                    fiber_push(fiber, result);
                    break;
                }

                case OP_PRINT: {
                    u8 count = READ_BYTE(frame);
                    RdxValue args[32];
                    for (i32 i = (i32)count - 1; i >= 0; i--) {
                        args[i] = fiber_pop(fiber);
                    }
                    ffi_invoke(fiber, FFI_IO_PRINT, args, count);
                    break;
                }

                case OP_SPAWN: {
                    u8 arg_count = READ_BYTE(frame);
                    RdxValue callee = fiber_peek(fiber, arg_count);

                    if (callee.type != VAL_FN || !callee.as.fn_val) {
                        rdx_print("[ERROR] Can only spawn functions in green thread fibers\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    ObjFunction *target_fn = callee.as.fn_val;
                    if (target_fn->arity != arg_count) {
                        rdx_print("[ERROR] Spawn expected ");
                        rdx_print_num((i64)target_fn->arity);
                        rdx_print(" arguments but got ");
                        rdx_print_num((i64)arg_count);
                        rdx_print("\n");
                        fiber->state = FIBER_ERROR;
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    RdxFiber *spawned = scheduler_spawn(&vm->sched, target_fn, fiber->capabilities);
                    if (spawned) {
                        /* Copy arguments into spawned fiber parameter slots */
                        for (u8 i = 0; i < arg_count; i++) {
                            RdxValue arg = fiber_peek(fiber, arg_count - 1 - i);
                            fiber_push(spawned, arg);
                        }
                        fiber->stack_top -= (arg_count + 1);
                        fiber_push(fiber, val_int(spawned->fiber_id));
                    } else {
                        fiber->stack_top -= (arg_count + 1);
                        fiber_push(fiber, val_nil());
                    }
                    break;
                }

                case OP_YIELD: {
                    scheduler_yield(&vm->sched);
                    break;
                }

                case OP_CAP_REQ: {
                    u16 name_idx = read_u16(frame);
                    RdxValue name_val = chunk->constants.values[name_idx];
                    if (name_val.type == VAL_STR && name_val.as.str_val) {
                        u64 cap_bit = cap_parse_name(name_val.as.str_val->chars, name_val.as.str_val->length);
                        /* Grant requested capability if VM possesses it */
                        if (cap_has_permission(vm->default_capabilities, cap_bit)) {
                            fiber->capabilities |= cap_bit;
                            fiber_push(fiber, val_cap(cap_bit));
                        } else {
                            rdx_print("[SECURITY] Requested capability denied by runtime policy\n");
                            fiber_push(fiber, val_nil());
                        }
                    }
                    break;
                }

                case OP_CAP_INVOKE: {
                    u8 op_id     = READ_BYTE(frame);
                    u8 arg_count = READ_BYTE(frame);
                    RdxValue args[16];
                    for (i32 i = (i32)arg_count - 1; i >= 0; i--) {
                        args[i] = fiber_pop(fiber);
                    }
                    RdxValue res = ffi_invoke(fiber, op_id, args, arg_count);
                    fiber_push(fiber, res);
                    break;
                }

                case OP_HALT: {
                    scheduler_finish_current(&vm->sched);
                    break;
                }

                default:
                    rdx_print("[ERROR] Unknown opcode in VM dispatch\n");
                    return INTERPRET_RUNTIME_ERROR;
            }
        }
    }

    return INTERPRET_OK;
}

InterpretResult vm_interpret_source(VM *vm, const char *source, usize length, const char *filename) {
    Arena compile_arena;
    arena_init(&compile_arena, 128 * 1024);

    Parser parser;
    parser_init(&parser, source, length, filename, &compile_arena);

    AstProgram *prog = parser_parse_program(&parser);
    if (parser.had_error || !prog) {
        arena_free(&compile_arena);
        return INTERPRET_COMPILE_ERROR;
    }

    ObjFunction *main_fn = compiler_compile_program(prog, &vm->vm_arena);
    if (!main_fn) {
        arena_free(&compile_arena);
        return INTERPRET_COMPILE_ERROR;
    }

    arena_free(&compile_arena);
    return vm_run_function(vm, main_fn);
}
