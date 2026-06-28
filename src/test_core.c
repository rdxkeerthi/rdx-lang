/*
 * test_core.c  —  Smoke test for the Windows RDX core.h
 * ======================================================
 * Compile (MinGW, VS Code Terminal / PowerShell):
 *
 *   gcc -nostdlib -O2 -o test_core.exe src/test_core.c -lkernel32
 *   .\test_core.exe
 *
 * Expected output:
 *   === RDX Windows core.h smoke test ===
 *   my_strlen("hello world") = 11
 *   my_strcmp("abc","abc")   = 0
 *   my_strcmp("abc","abd")   = -1
 *   print_num(42)            = 42
 *   print_num(-9001)         = -9001
 *   print_hex(0xDEAD)        = 0xdead
 *   VirtualAlloc block       = OK
 *   Bump alloc block         = OK
 *   my_strcat test           = Hello, RDX!
 *   === All tests passed ===
 *
 * This file defines RDX_DEFINE_ENTRY so that core.h emits
 * the mainCRTStartup entry point.  It also defines a stub
 * rdx_compiler_main() so the linker is satisfied.
 */

/* Pull in the entry point from core.h */
#define RDX_DEFINE_ENTRY
#include "core.h"

/* -------------------------------------------------------
 * rdx_compiler_main — the "real" compiler entry point.
 * For the smoke test this is just our test suite.
 * ------------------------------------------------------- */
int rdx_compiler_main(void) {

    rdx_print("=== RDX Windows core.h smoke test ===\r\n");

    /* ---- my_strlen ---------------------------------------- */
    usize l = my_strlen("hello world");
    rdx_print("my_strlen(\"hello world\") = ");
    rdx_print_num((i64)l);
    rdx_print("\r\n");

    /* ---- my_strcmp ---------------------------------------- */
    i32 c1 = my_strcmp("abc", "abc");
    rdx_print("my_strcmp(\"abc\",\"abc\")   = ");
    rdx_print_num((i64)c1);
    rdx_print("\r\n");

    i32 c2 = my_strcmp("abc", "abd");
    rdx_print("my_strcmp(\"abc\",\"abd\")   = ");
    rdx_print_num((i64)c2);
    rdx_print("\r\n");

    /* ---- rdx_print_num ------------------------------------ */
    rdx_print("print_num(42)            = ");
    rdx_print_num(42);
    rdx_print("\r\n");

    rdx_print("print_num(-9001)         = ");
    rdx_print_num(-9001);
    rdx_print("\r\n");

    /* ---- rdx_print_hex ------------------------------------ */
    rdx_print("print_hex(0xDEAD)        = ");
    rdx_print_hex(0xDEAD);
    rdx_print("\r\n");

    /* ---- VirtualAlloc / VirtualFree (rdx_va_alloc) -------- */
    void *blk = rdx_va_alloc(256);
    if (blk == NULL) {
        rdx_panic("rdx_va_alloc returned NULL");
    }
    /* Write a pattern and read it back */
    my_memset(blk, 0xBE, 256);
    u8 *pb = (u8*)blk;
    if (pb[0] != 0xBE || pb[127] != 0xBE || pb[255] != 0xBE) {
        rdx_panic("VirtualAlloc memory pattern mismatch");
    }
    rdx_va_free(blk);
    rdx_print("VirtualAlloc block       = OK\r\n");

    /* ---- Bump allocator ----------------------------------- */
    char *scratch = (char*)rdx_alloc_bump(128);
    if (scratch == NULL) {
        rdx_panic("rdx_alloc_bump returned NULL");
    }
    my_strcpy(scratch, "bump allocation test string");
    if (my_strcmp(scratch, "bump allocation test string") != 0) {
        rdx_panic("bump alloc content mismatch");
    }
    rdx_print("Bump alloc block         = OK\r\n");

    /* ---- my_strcat ---------------------------------------- */
    char cat_buf[64];
    my_strcpy(cat_buf, "Hello");
    my_strcat(cat_buf, ", RDX!");
    rdx_print("my_strcat test           = ");
    rdx_print(cat_buf);
    rdx_print("\r\n");

    rdx_print("=== All tests passed ===\r\n");
    return 0;
}
