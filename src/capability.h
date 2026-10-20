/*
 * ============================================================================
 *  capability.h  —  RDX Zero-Trust Capability-Based Security Model
 * ============================================================================
 *  The VM has zero implicit access to the host OS.
 *  Host operations (File I/O, Network, OS Processes) require explicit
 *  Capability Tokens passed to or held by the executing Fiber.
 * ============================================================================
 */

#ifndef RDX_CAPABILITY_H
#define RDX_CAPABILITY_H

#include "core.h"

/* Capability permission bitmask */
typedef enum {
    CAP_NONE          = 0,
    CAP_STDIO_WRITE   = (1ULL << 0),  /* Console output                     */
    CAP_STDIO_READ    = (1ULL << 1),  /* Console input                      */
    CAP_FILE_READ     = (1ULL << 2),  /* Host filesystem read               */
    CAP_FILE_WRITE    = (1ULL << 3),  /* Host filesystem write              */
    CAP_NET_SOCKET    = (1ULL << 4),  /* Network socket communication       */
    CAP_PROC_SPAWN    = (1ULL << 5),  /* Spawn OS child processes           */
    CAP_MEM_EXPAND    = (1ULL << 6),  /* Allocate virtual memory outside task*/
    CAP_SYS_ALL       = 0xFFFFFFFFFFFFFFFFULL,
} CapPermission;

typedef struct {
    u64 permissions;
    u64 token_id;
    u32 valid_until_fiber;
} CapToken;

u64  cap_parse_name(const char *name, usize length);
bool cap_has_permission(u64 current_caps, u64 required_cap);
const char *cap_to_string(u64 single_cap);

#endif /* RDX_CAPABILITY_H */
