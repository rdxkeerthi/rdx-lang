/*
 * ============================================================================
 *  ffi.c  —  RDX Zero-Trust Capability-Verified FFI Bridge Implementation
 * ============================================================================
 */

#include "ffi.h"

bool ffi_check_capability(const RdxFiber *fiber, u64 required_cap, const char *op_name) {
    if (!fiber) return FALSE;
    if (!cap_has_permission(fiber->capabilities, required_cap)) {
        rdx_print("[SECURITY TRAP] Access Denied: Fiber ");
        rdx_print_num((i64)fiber->fiber_id);
        rdx_print(" lacks required capability '");
        rdx_print(cap_to_string(required_cap));
        rdx_print("' for operation '");
        rdx_print(op_name);
        rdx_print("'\n");
        return FALSE;
    }
    return TRUE;
}

RdxValue ffi_invoke(RdxFiber *fiber, u8 op_id, RdxValue *args, u8 arg_count) {
    switch (op_id) {
        case FFI_IO_PRINT: {
            if (!ffi_check_capability(fiber, CAP_STDIO_WRITE, "print")) {
                return val_nil();
            }
            for (u8 i = 0; i < arg_count; i++) {
                value_print(args[i]);
                if (i + 1 < arg_count) rdx_print(" ");
            }
            rdx_print("\n");
            return val_nil();
        }

        case FFI_FILE_READ: {
            if (!ffi_check_capability(fiber, CAP_FILE_READ, "file_read")) {
                return val_nil();
            }
            if (arg_count < 1 || args[0].type != VAL_STR || !args[0].as.str_val) {
                return val_nil();
            }
            const char *path = args[0].as.str_val->chars;
            usize file_size = 0;
            char *content = rdx_read_file(path, &file_size);
            if (!content) return val_nil();
            ObjString *str = obj_string_create(&fiber->task_arena, content, file_size);
            return val_str(str);
        }

        case FFI_FILE_WRITE: {
            if (!ffi_check_capability(fiber, CAP_FILE_WRITE, "file_write")) {
                return val_nil();
            }
            if (arg_count < 2 || args[0].type != VAL_STR || args[1].type != VAL_STR) {
                return val_nil();
            }
            const char *path = args[0].as.str_val->chars;
            const char *data = args[1].as.str_val->chars;
            usize data_len = args[1].as.str_val->length;
            bool ok = rdx_write_file(path, data, data_len);
            return val_bool(ok);
        }

        default:
            rdx_print("[ERROR] Unknown FFI Operation ID\n");
            return val_nil();
    }
}
