#ifndef RDX_PLATFORM_LINUX_IMPL_H
#define RDX_PLATFORM_LINUX_IMPL_H

/* ============================================================
 *  §2  Linux Type Aliases
 * ============================================================ */

/* We use the types defined in core.h (§1b), so we don't need much here. */

/* ============================================================
 *  §3  Linux Constants
 * ============================================================ */

#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define MAP_PRIVATE     0x02
#define MAP_ANONYMOUS   0x20
#define MAP_FAILED      ((void *)-1)

#define O_RDONLY        00000000
#define O_WRONLY        00000001
#define O_CREAT         00000100
#define O_TRUNC         00001000

/* ============================================================
 *  §4  Linux Syscall Wrappers (x86_64)
 * ============================================================ */

static inline i64 rdx_syscall0(i64 n) {
    i64 ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
    return ret;
}

static inline i64 rdx_syscall1(i64 n, i64 a1) {
    i64 ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline i64 rdx_syscall2(i64 n, i64 a1, i64 a2) {
    i64 ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline i64 rdx_syscall3(i64 n, i64 a1, i64 a2, i64 a3) {
    i64 ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline i64 rdx_syscall4(i64 n, i64 a1, i64 a2, i64 a3, i64 a4) {
    i64 ret;
    register i64 r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

static inline i64 rdx_syscall6(i64 n, i64 a1, i64 a2, i64 a3, i64 a4, i64 a5, i64 a6) {
    i64 ret;
    register i64 r10 __asm__("r10") = a4;
    register i64 r8 __asm__("r8") = a5;
    register i64 r9 __asm__("r9") = a6;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

/* ============================================================
 *  §6  I/O Helpers
 * ============================================================ */

#ifdef RDX_DEFINE_ENTRY
  i64 rdx_stdout = 1;
  i64 rdx_stderr = 2;
  i64 rdx_stdin  = 0;
  u8 *rdx_bump_base = 0;
  u8 *rdx_bump_ptr  = 0;
  u8 *rdx_bump_end  = 0;
#else
  extern i64 rdx_stdout;
  extern i64 rdx_stderr;
  extern i64 rdx_stdin;
  extern u8 *rdx_bump_base;
  extern u8 *rdx_bump_ptr;
  extern u8 *rdx_bump_end;
#endif

static inline void rdx_write_raw(i64 fd, const char *buf, usize len) {
    usize written = 0;
    while (written < len) {
        i64 res = rdx_syscall3(1, fd, (i64)(buf + written), len - written);
        if (res <= 0) break;
        written += (usize)res;
    }
}

static inline void rdx_print(const char *s) {
    rdx_write_raw(rdx_stdout, s, my_strlen(s));
}

static inline void rdx_print_err(const char *s) {
    rdx_write_raw(rdx_stderr, s, my_strlen(s));
}

static inline int rdx_read_line(char *buf, int max_chars) {
    i64 res = rdx_syscall3(0, rdx_stdin, (i64)buf, max_chars - 1);
    if (res <= 0) return 0;
    int len = (int)res;
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) len--;
    buf[len] = '\0';
    return len;
}

static inline void rdx_print_char(char c) {
    rdx_write_raw(rdx_stdout, &c, 1);
}

static inline void rdx_print_num(i64 n) {
    char  buf[21];
    char *p = buf + 20;
    *p = '\0';
    if (n < 0) {
        u64 un = (u64)(-(n + 1)) + 1ULL;
        do { *--p = '0' + (char)(un % 10); un /= 10; } while (un);
        *--p = '-';
    } else {
        u64 un = (u64)n;
        do { *--p = '0' + (char)(un % 10); un /= 10; } while (un);
    }
    rdx_write_raw(rdx_stdout, p, (usize)(buf + 20 - p));
}

static inline void rdx_print_num_err(i64 n) {
    char  buf[21];
    char *p = buf + 20;
    *p = '\0';
    if (n < 0) {
        u64 un = (u64)(-(n + 1)) + 1ULL;
        do { *--p = '0' + (char)(un % 10); un /= 10; } while (un);
        *--p = '-';
    } else {
        u64 un = (u64)n;
        do { *--p = '0' + (char)(un % 10); un /= 10; } while (un);
    }
    rdx_write_raw(rdx_stderr, p, (usize)(buf + 20 - p));
}

static inline void rdx_print_hex(u64 n) {
    static const char hex[] = "0123456789abcdef";
    char  buf[19];
    char *p = buf + 18;
    *p = '\0';
    do { *--p = hex[n & 0xF]; n >>= 4; } while (n);
    *--p = 'x';
    *--p = '0';
    rdx_write_raw(rdx_stdout, p, (usize)(buf + 18 - p));
}

static inline void rdx_exit(i32 code) {
    rdx_syscall1(60, (i64)code);
    while(1);
}

static inline void rdx_panic(const char *msg) {
    rdx_print_err("[RDX PANIC] ");
    rdx_print_err(msg);
    rdx_print_err("\n");
    rdx_exit(1);
}

static inline int rdx_run_cmd(const char *cmd) {
    i64 pid = rdx_syscall0(57); /* fork */
    if (pid == 0) {
        const char *argv[] = { "/bin/sh", "-c", cmd, 0 };
        const char *envp[] = { 0 };
        rdx_syscall3(59, (i64)"/bin/sh", (i64)argv, (i64)envp); /* execve */
        rdx_syscall1(60, 127);
    } else if (pid > 0) {
        int status = 0;
        rdx_syscall4(61, pid, (i64)&status, 0, 0); /* wait4 */
        if ((status & 0x7f) == 0) {
            return (status & 0xff00) >> 8;
        }
        return -1;
    }
    return -1;
}

/* ============================================================
 *  §7  Memory Allocator
 * ============================================================ */

/* Bump-allocator arena size (128 MiB — plenty for any source file) */
#define RDX_BUMP_ARENA_SIZE  (128ULL * 1024ULL * 1024ULL)
#define RDX_ALIGN  16ULL

static inline usize rdx_align_up(usize n, usize align) {
    return (n + align - 1) & ~(align - 1);
}

static inline void rdx_heap_init(void) {
    /* mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) */
    i64 res = rdx_syscall6(9, 0, RDX_BUMP_ARENA_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (res < 0) rdx_exit(2);
    rdx_bump_base = (u8*)res;
    rdx_bump_ptr  = (u8*)res;
    rdx_bump_end  = (u8*)res + RDX_BUMP_ARENA_SIZE;
}

static inline void *rdx_alloc_bump(usize size) {
    if (size == 0) return 0;
    if (rdx_bump_base == 0) rdx_heap_init();
    usize aligned = rdx_align_up(size, RDX_ALIGN);
    u8 *result = rdx_bump_ptr;
    u8 *new_ptr = rdx_bump_ptr + aligned;
    if (new_ptr > rdx_bump_end) return 0;
    rdx_bump_ptr = new_ptr;
    my_memset(result, 0, aligned);
    return result;
}

static inline void rdx_heap_reset(void) {
    rdx_bump_ptr = rdx_bump_base;
}

static inline void *rdx_va_alloc(usize size) {
    if (size == 0) return 0;
    usize total = rdx_align_up(size + 8, 4096ULL);
    i64 res = rdx_syscall6(9, 0, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (res < 0) return 0;
    *((u64*)res) = (u64)total;
    return (u8*)res + 8;
}

static inline void rdx_va_free(void *ptr) {
    if (ptr == 0) return;
    void *base = (u8*)ptr - 8;
    usize total = (usize)(*((u64*)base));
    /* munmap(base, total) */
    rdx_syscall2(11, (i64)base, total);
}

static inline void *rdx_alloc(usize size) {
    return rdx_va_alloc(size);
}

/* ============================================================
 *  §8  File I/O Helpers
 * ============================================================ */

static inline char *rdx_read_file(const char *path, usize *out_len) {
    /* open(path, O_RDONLY, 0) */
    i64 fd = rdx_syscall3(2, (i64)path, O_RDONLY, 0);
    if (fd < 0) {
        rdx_print_err("[RDX] Cannot open file: ");
        rdx_print_err(path);
        rdx_print_err("\n");
        return 0;
    }

    /* fstat(fd, &statbuf) - we need to allocate statbuf.
       struct stat is usually 144 bytes on x86_64 linux.
       Size is at offset 48 (st_size). */
    u8 statbuf[144];
    my_memset(statbuf, 0, sizeof(statbuf));
    if (rdx_syscall2(5, fd, (i64)statbuf) < 0) {
        rdx_syscall1(3, fd);
        rdx_print_err("[RDX] Cannot stat file\n");
        return 0;
    }

    i64 file_size = *(i64*)(statbuf + 48);
    if (file_size < 0 || (u64)file_size >= MAX_SOURCE_SIZE) {
        rdx_syscall1(3, fd);
        rdx_print_err("[RDX] File too large\n");
        return 0;
    }

    usize sz = (usize)file_size;
    char *buf = (char*)rdx_alloc_bump(sz + 1);
    if (!buf) {
        rdx_syscall1(3, fd);
        rdx_print_err("[RDX] OOM reading file\n");
        return 0;
    }

    i64 bytes_read = rdx_syscall3(0, fd, (i64)buf, sz);
    rdx_syscall1(3, fd);

    if (bytes_read < 0) return 0;
    buf[bytes_read] = '\0';
    if (out_len) *out_len = (usize)bytes_read;
    return buf;
}

static inline i32 rdx_write_file(const char *path, const char *data, usize len) {
    /* open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) */
    i64 fd = rdx_syscall3(2, (i64)path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        rdx_print_err("[RDX] Cannot create file: ");
        rdx_print_err(path);
        rdx_print_err("\n");
        return -1;
    }
    usize written = 0;
    while (written < len) {
        i64 w = rdx_syscall3(1, fd, (i64)(data + written), len - written);
        if (w <= 0) {
            rdx_syscall1(3, fd);
            return -1;
        }
        written += (usize)w;
    }
    rdx_syscall1(3, fd);
    return 0;
}

/* ============================================================
 *  §10  Linux Entry Point
 * ============================================================ */

#ifdef RDX_DEFINE_ENTRY

extern int rdx_compiler_main(void);

void _start(void) {
    rdx_heap_init();
    int exit_code = rdx_compiler_main();
    rdx_exit(exit_code);
}

#endif  /* RDX_DEFINE_ENTRY */

#endif // RDX_PLATFORM_LINUX_IMPL_H
