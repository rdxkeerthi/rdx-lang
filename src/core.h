/*
 * ============================================================================
 *  core.h  —  RDX Compiler: Freestanding Windows Core Library
 * ============================================================================
 *  Target     : x86-64 Windows (Win64 ABI)
 *  Toolchain  : MinGW-w64 GCC  +  NASM
 *  Compile    : gcc -nostdlib -O2 -o rdxc.exe src/main.c -lkernel32
 *
 *  Zero dependency on MSVCRT, CRT, <stdio.h>, <stdlib.h>, or <string.h>.
 *  All Windows interaction is via direct imports from kernel32.dll,
 *  declared here manually with correct __stdcall / __attribute__ signatures.
 *
 *  Contents:
 *    §1   Primitive type aliases (no <stdint.h>)
 *    §2   Windows type aliases  (HANDLE, DWORD, BOOL, …)
 *    §3   Win32 constants       (STD_HANDLE values, VirtualAlloc flags, …)
 *    §4   kernel32.dll function declarations (no header needed)
 *    §5   String utilities      (my_strlen, my_strcmp, my_strcpy, …)
 *    §6   I/O helpers           (rdx_print, rdx_print_num, rdx_panic, …)
 *    §7   Memory allocator      (VirtualAlloc / VirtualFree)
 *    §8   File I/O helpers      (rdx_read_file, rdx_write_file)
 *    §9   Compiler-global limits
 *    §10  Entry point           (mainCRTStartup — required by MinGW -nostdlib)
 * ============================================================================
 */

#ifndef RDX_CORE_H
#define RDX_CORE_H

/* ============================================================
 *  §1  Primitive Type Aliases  (no <stdint.h>)
 * ============================================================ */

typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef signed char         i8;
typedef signed short        i16;
typedef signed int          i32;
typedef signed long long    i64;

typedef unsigned long long  usize;    /* size equivalent on Win64 */
typedef long long           isize;

/* bool — guard against GCC's built-in _Bool/bool in C99+ */
#ifndef __cplusplus
#ifndef bool
#define bool i32
#endif
#endif
#ifndef TRUE
#define TRUE   1
#endif
#ifndef FALSE
#define FALSE  0
#endif
#ifndef NULL
#define NULL   ((void*)0)
#endif

/* ============================================================
 *  §1b  Compiler-Global Limits  (must be above rdx_read_file)
 * ============================================================ */

#define MAX_TOKENS        131072UL
#define MAX_NODES         524288UL
#define MAX_IDENT_LEN     255UL
#define MAX_STR_LIT_LEN   4096UL
#define MAX_SOURCE_SIZE   (1024ULL * 1024ULL)       /* 1 MiB  */
#define MAX_EMIT_SIZE     (4096ULL * 1024ULL)       /* 4 MiB  */

/* ============================================================
 *  §5  String Utility Functions  (no <string.h>)
 * ============================================================ */

/*
 * my_strlen — returns number of bytes before NUL terminator.
 */
static inline usize my_strlen(const char *s) {
    usize n = 0;
    while (s[n] != '\0') n++;
    return n;
}

/*
 * my_strcmp — lexicographic comparison.
 *   Returns 0 if equal, <0 if a < b, >0 if a > b.
 */
static inline i32 my_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/*
 * my_strncmp — bounded comparison, at most n bytes.
 */
static inline i32 my_strncmp(const char *a, const char *b, usize n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (n == 0) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

/*
 * my_strcpy — copies src into dst including NUL.  Returns dst.
 */
static inline char *my_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != '\0');
    return dst;
}

/*
 * my_strncpy — copies at most n bytes from src into dst; pads with NUL.
 */
static inline char *my_strncpy(char *dst, const char *src, usize n) {
    char *d = dst;
    while (n && (*d++ = *src++) != '\0') n--;
    while (n--) *d++ = '\0';
    return dst;
}

/*
 * my_strcat — appends src to end of dst.  Returns dst.
 */
static inline char *my_strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dst;
}

/*
 * my_memcpy — copies n bytes from src to dst (non-overlapping).
 */
static inline void *my_memcpy(void *dst, const void *src, usize n) {
    u8       *d = (u8*)dst;
    const u8 *s = (const u8*)src;
    while (n--) *d++ = *s++;
    return dst;
}

/*
 * my_memset — fills n bytes at ptr with byte value c.
 */
static inline void *my_memset(void *ptr, i32 c, usize n) {
    u8 *p = (u8*)ptr;
    while (n--) *p++ = (u8)c;
    return ptr;
}


#include "platform/interface.h"


#endif  /* RDX_CORE_H */
