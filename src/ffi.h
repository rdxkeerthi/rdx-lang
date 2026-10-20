/*
 * ============================================================================
 *  ffi.h  —  RDX Zero-Trust Capability-Verified FFI Bridge
 * ============================================================================
 */

#ifndef RDX_FFI_H
#define RDX_FFI_H

#include "core.h"
#include "fiber.h"
#include "value.h"
#include "capability.h"

typedef enum {
    FFI_IO_PRINT,
    FFI_FILE_READ,
    FFI_FILE_WRITE,
    FFI_NET_CONNECT,
    FFI_PROC_SPAWN,
} FfiOperation;

bool     ffi_check_capability(const RdxFiber *fiber, u64 required_cap, const char *op_name);
RdxValue ffi_invoke(RdxFiber *fiber, u8 op_id, RdxValue *args, u8 arg_count);

#endif /* RDX_FFI_H */
