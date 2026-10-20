/*
 * ============================================================================
 *  capability.c  —  RDX Capability Security Implementation
 * ============================================================================
 */

#include "capability.h"

u64 cap_parse_name(const char *name, usize length) {
    #define MATCH_CAP(str, bit) \
        if (length == sizeof(str)-1 && my_strncmp(name, str, sizeof(str)-1) == 0) return bit

    MATCH_CAP("stdio.write",  CAP_STDIO_WRITE);
    MATCH_CAP("stdio.read",   CAP_STDIO_READ);
    MATCH_CAP("stdio",        CAP_STDIO_WRITE | CAP_STDIO_READ);
    MATCH_CAP("file.read",    CAP_FILE_READ);
    MATCH_CAP("file.write",   CAP_FILE_WRITE);
    MATCH_CAP("file",         CAP_FILE_READ | CAP_FILE_WRITE);
    MATCH_CAP("net.socket",   CAP_NET_SOCKET);
    MATCH_CAP("net",          CAP_NET_SOCKET);
    MATCH_CAP("process.spawn",CAP_PROC_SPAWN);
    MATCH_CAP("all",          CAP_SYS_ALL);
    MATCH_CAP("sys",          CAP_SYS_ALL);

    #undef MATCH_CAP
    return CAP_NONE;
}

bool cap_has_permission(u64 current_caps, u64 required_cap) {
    return (current_caps & required_cap) == required_cap;
}

const char *cap_to_string(u64 single_cap) {
    switch (single_cap) {
        case CAP_STDIO_WRITE: return "stdio.write";
        case CAP_STDIO_READ:  return "stdio.read";
        case CAP_FILE_READ:   return "file.read";
        case CAP_FILE_WRITE:  return "file.write";
        case CAP_NET_SOCKET:  return "net.socket";
        case CAP_PROC_SPAWN:  return "process.spawn";
        default:              return "unknown";
    }
}
