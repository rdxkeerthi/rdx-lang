#ifndef RDX_PLATFORM_WINDOWS_IMPL_H
#define RDX_PLATFORM_WINDOWS_IMPL_H

/* ============================================================
 *  §2  Windows Type Aliases
 * ============================================================ */

#ifndef WIN_IMPORT
#define WIN_IMPORT __stdcall
#endif

typedef unsigned int        DWORD;
typedef unsigned short      WORD;
typedef long long           LONGLONG;
typedef void*               HANDLE;
typedef void*               LPVOID;
typedef const void*         LPCVOID;
typedef char*               LPSTR;
typedef const char*         LPCSTR;
typedef int                 BOOL;
typedef unsigned long long  SIZE_T;
typedef DWORD*              LPDWORD;
typedef unsigned long       ULONG_PTR;

/* ============================================================
 *  §3  Win32 Constants
 * ============================================================ */

/* Standard handle identifiers */
#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_ERROR_HANDLE    ((DWORD)-12)

/* Invalid handle */
#define INVALID_HANDLE_VALUE  ((HANDLE)(LONGLONG)-1)

/* VirtualAlloc allocation types */
#define MEM_COMMIT            0x00001000UL
#define MEM_RESERVE           0x00002000UL
#define MEM_COMMIT_RESERVE    (MEM_COMMIT | MEM_RESERVE)

/* VirtualFree free types */
#define MEM_RELEASE           0x00008000UL
#define MEM_DECOMMIT          0x00004000UL

/* Page protection flags */
#define PAGE_READWRITE        0x04UL
#define PAGE_READONLY         0x02UL
#define PAGE_NOACCESS         0x01UL

/* CreateFileA access/share/disposition */
#define GENERIC_READ          0x80000000UL
#define GENERIC_WRITE         0x40000000UL
#define FILE_SHARE_READ       0x00000001UL
#define FILE_SHARE_WRITE      0x00000002UL
#define OPEN_EXISTING         3UL
#define CREATE_ALWAYS         2UL
#define CREATE_NEW            1UL
#define OPEN_ALWAYS           4UL
#define FILE_ATTRIBUTE_NORMAL 0x80UL

/* GetLastError codes we care about */
#define ERROR_SUCCESS         0UL

/* ============================================================
 *  §4  kernel32.dll Function Declarations
 * VirtualAlloc — reserves, commits, or changes a region of virtual memory.
 *   lpAddress   : desired start address (NULL = let OS choose)
 *   dwSize      : size in bytes
 *   flAllocType : MEM_COMMIT | MEM_RESERVE
 *   flProtect   : PAGE_READWRITE
 *   Returns pointer to allocated region, or NULL on failure.
 */
extern LPVOID WIN_IMPORT VirtualAlloc(
    LPVOID  lpAddress,
    SIZE_T  dwSize,
    DWORD   flAllocationType,
    DWORD   flProtect
);

/*
 * VirtualFree — releases or decommits a region of virtual memory.
 *   lpAddress  : base address returned by VirtualAlloc
 *   dwSize     : 0 when dwFreeType == MEM_RELEASE
 *   dwFreeType : MEM_RELEASE or MEM_DECOMMIT
 */
extern BOOL WIN_IMPORT VirtualFree(
    LPVOID lpAddress,
    SIZE_T dwSize,
    DWORD  dwFreeType
);

/* --- Dynamic Loading --- */
typedef void* HMODULE;
extern HMODULE WIN_IMPORT LoadLibraryA(LPCSTR lpLibFileName);
extern void* WIN_IMPORT GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
extern BOOL WIN_IMPORT FreeLibrary(HMODULE hLibModule);

/* --- Process Control --- */

/*
 * ExitProcess — ends the calling process.
 */
extern void WIN_IMPORT ExitProcess(DWORD uExitCode);

/*
 * GetLastError — retrieves the last error code for the calling thread.
 */
extern DWORD WIN_IMPORT GetLastError(void);

/*
 * WaitForSingleObject — wait for process to finish
 */
extern DWORD WIN_IMPORT WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);

#define INFINITE 0xFFFFFFFFUL

/*
 * GetExitCodeProcess
 */
extern BOOL WIN_IMPORT GetExitCodeProcess(HANDLE hProcess, LPDWORD lpExitCode);

typedef struct {
    HANDLE hProcess;
    HANDLE hThread;
    DWORD  dwProcessId;
    DWORD  dwThreadId;
} PROCESS_INFORMATION;

typedef struct {
    DWORD  cb;
    LPSTR  lpReserved;
    LPSTR  lpDesktop;
    LPSTR  lpTitle;
    DWORD  dwX;
    DWORD  dwY;
    DWORD  dwXSize;
    DWORD  dwYSize;
    DWORD  dwXCountChars;
    DWORD  dwYCountChars;
    DWORD  dwFillAttribute;
    DWORD  dwFlags;
    WORD   wShowWindow;
    WORD   cbReserved2;
    LPVOID lpReserved2;
    HANDLE hStdInput;
    HANDLE hStdOutput;
    HANDLE hStdError;
} STARTUPINFOA;

/*
 * CreateProcessA — starts a new process
 */
extern BOOL WIN_IMPORT CreateProcessA(
    LPCSTR                lpApplicationName,
    LPSTR                 lpCommandLine,
    LPVOID                lpProcessAttributes,
    LPVOID                lpThreadAttributes,
    BOOL                  bInheritHandles,
    DWORD                 dwCreationFlags,
    LPVOID                lpEnvironment,
    LPCSTR                lpCurrentDirectory,
    STARTUPINFOA         *lpStartupInfo,
    PROCESS_INFORMATION  *lpProcessInformation
);

/*
 * DeleteFileA — deletes an existing file
 */
extern BOOL WIN_IMPORT DeleteFileA(LPCSTR lpFileName);

extern HANDLE WIN_IMPORT GetStdHandle(DWORD nStdHandle);
extern BOOL WIN_IMPORT WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped);
extern BOOL WIN_IMPORT CloseHandle(HANDLE hObject);
extern HANDLE WIN_IMPORT CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPVOID lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
extern BOOL WIN_IMPORT GetFileSizeEx(HANDLE hFile, LONGLONG *lpFileSize);
extern BOOL WIN_IMPORT ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped);

/* ============================================================
 *  §6  I/O Helpers
 *
 *  We cache the stdout and stderr HANDLE values at startup
 *  (see §10 mainCRTStartup) so that every print call avoids
 *  an extra GetStdHandle() round-trip.
 * ============================================================ */

/* Cached Win32 standard handles — defined once in main.c (RDX_DEFINE_ENTRY).
 * Every other TU sees them as extern. */
#ifdef RDX_DEFINE_ENTRY
  HANDLE rdx_stdout = (HANDLE)-1LL;
  HANDLE rdx_stderr = (HANDLE)-1LL;
  HANDLE rdx_stdin  = (HANDLE)-1LL;
  u8    *rdx_bump_base = 0;
  u8    *rdx_bump_ptr  = 0;
  u8    *rdx_bump_end  = 0;
#else
  extern HANDLE rdx_stdout;
  extern HANDLE rdx_stderr;
  extern HANDLE rdx_stdin;
  extern u8    *rdx_bump_base;
  extern u8    *rdx_bump_ptr;
  extern u8    *rdx_bump_end;
#endif

/*
 * rdx_write_raw — writes exactly `len` bytes from `buf` to `handle`.
 *                 Loops until all bytes are written (WriteFile may
 *                 write fewer bytes than requested in rare cases).
 */
static inline void rdx_write_raw(HANDLE handle,
                                  const char *buf, usize len)
{
    DWORD written;
    while (len > 0) {
        DWORD chunk = (len > 0x7FFFFFFFUL) ? 0x7FFFFFFFUL : (DWORD)len;
        if (!WriteFile(handle, buf, chunk, &written, NULL)) break;
        if (written == 0) break;
        buf += written;
        len -= written;
    }
}

/*
 * rdx_print — writes a NUL-terminated string to stdout.
 */
static inline void rdx_print(const char *s) {
    rdx_write_raw(rdx_stdout, s, my_strlen(s));
}

/*
 * rdx_print_err — writes a NUL-terminated string to stderr.
 */
static inline void rdx_print_err(const char *s) {
    rdx_write_raw(rdx_stderr, s, my_strlen(s));
}

extern BOOL WIN_IMPORT SetConsoleMode(HANDLE hConsoleHandle, DWORD dwMode);
extern BOOL WIN_IMPORT ReadConsoleA(
    HANDLE       hConsoleInput,
    LPVOID       lpBuffer,
    DWORD        nNumberOfCharsToRead,
    LPDWORD      lpNumberOfCharsRead,
    LPVOID       pInputControl
);

/*
 * rdx_read_line — reads one line of input from stdin.
 */
static inline int rdx_read_line(char *buf, int max_chars) {
    DWORD chars_read = 0;
    SetConsoleMode(rdx_stdin, 0x0003);  /* ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT */
    ReadConsoleA(rdx_stdin, buf, (DWORD)(max_chars - 1), &chars_read, NULL);
    int len = (int)chars_read;
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) len--;
    buf[len] = '\0';
    return len;
}

/*
 * rdx_print_char — writes a single character to stdout.
 */
static inline void rdx_print_char(char c) {
    rdx_write_raw(rdx_stdout, &c, 1);
}
/*
 * rdx_print_num — converts a signed 64-bit integer to decimal text
 *                 and writes it to stdout.  No printf / itoa needed.
 */
static inline void rdx_print_num(i64 n) {
    char  buf[21];        /* max 20 decimal digits + sign */
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

/*
 * rdx_print_hex — writes an unsigned 64-bit integer as "0x..." hex.
 */
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

/*
 * rdx_exit — terminates the process with the given exit code.
 *            This is a noreturn call; ExitProcess never returns.
 */
static inline void rdx_exit(i32 code) {
    ExitProcess((DWORD)code);
    /* unreachable */
    while (1);
}

/*
 * rdx_panic — prints "[RDX PANIC] msg" to stderr and exits with code 1.
 */
static inline void rdx_panic(const char *msg) {
    rdx_print_err("[RDX PANIC] ");
    rdx_print_err(msg);
    rdx_print_err("\r\n");
    rdx_exit(1);
}

/*
 * rdx_run_cmd — executes a command synchronously, returns exit code.
 */
static inline int rdx_run_cmd(const char *cmd) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    my_memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    my_memset(&pi, 0, sizeof(pi));

    char buf[1024];
    my_strncpy(buf, cmd, 1023);
    buf[1023] = '\0';

    if (!CreateProcessA(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)exit_code;
}

/* ============================================================
 *  §7  Memory Allocator  —  VirtualAlloc / VirtualFree
 *
 *  Two layers:
 *
 *  A) BUMP ALLOCATOR  — fast O(1) allocations for the compiler's
 *     internal data structures (token arrays, AST node pools,
 *     string scratch buffers).  Backed by a single large
 *     VirtualAlloc reservation.  No per-block free; reset with
 *     rdx_heap_reset().
 *
 *  B) PRECISE ALLOCATOR — for RDX `alloc` / `drop` keywords.
 *     Each block is an independent VirtualAlloc call so that
 *     VirtualFree can release it exactly.  The allocated size
 *     is stashed in the first 8 bytes so rdx_va_free() works
 *     from a single user pointer.
 * ============================================================ */

/* Bump-allocator arena size (128 MiB — plenty for any source file) */
#define RDX_BUMP_ARENA_SIZE  (128ULL * 1024ULL * 1024ULL)

/* Alignment for all allocations */
#define RDX_ALIGN  16ULL

static u8    *rdx_bump_base_unused = 0; /* kept for alignment — real vars extern above */

/*
 * rdx_align_up — round n up to the next multiple of align (power of 2).
 */
static inline usize rdx_align_up(usize n, usize align) {
    return (n + align - 1) & ~(align - 1);
}

/*
 * rdx_heap_init — initialises the bump allocator arena.
 *                 Must be called once before rdx_alloc_bump().
 *                 Called automatically by mainCRTStartup (§10).
 */
static inline void rdx_heap_init(void) {
    void *mem = VirtualAlloc(
        NULL,
        RDX_BUMP_ARENA_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (mem == NULL) {
        /* Cannot even initialise the heap — nothing we can do */
        ExitProcess(2);
    }
    rdx_bump_base = (u8*)mem;
    rdx_bump_ptr  = (u8*)mem;
    rdx_bump_end  = (u8*)mem + RDX_BUMP_ARENA_SIZE;
}

/*
 * rdx_alloc_bump — bump-allocates `size` bytes, aligned to RDX_ALIGN.
 *                  Returns a zeroed pointer, or NULL if arena is full.
 *
 *  Use for: token arrays, AST node pools, scratch string buffers.
 */
static inline void *rdx_alloc_bump(usize size) {
    if (size == 0) return NULL;
    if (rdx_bump_base == NULL) rdx_heap_init();

    usize aligned = rdx_align_up(size, RDX_ALIGN);
    u8   *result  = rdx_bump_ptr;
    u8   *new_ptr = rdx_bump_ptr + aligned;

    if (new_ptr > rdx_bump_end) return NULL;   /* arena exhausted */

    rdx_bump_ptr = new_ptr;
    my_memset(result, 0, aligned);
    return result;
}

/*
 * rdx_heap_reset — resets the bump pointer to base.
 *                  Invalidates all previous bump allocations at once.
 *                  Does NOT release memory to the OS.
 */
static inline void rdx_heap_reset(void) {
    rdx_bump_ptr = rdx_bump_base;
}

/*
 * rdx_va_alloc — allocates `size` bytes via VirtualAlloc.
 *
 *  Layout of the returned region:
 *    [0..7]    : u64 — total committed size (for rdx_va_free)
 *    [8..end]  : user data (zero-initialised by VirtualAlloc)
 *
 *  Returns pointer to user data (8 bytes past the header), or NULL.
 *
 *  Use for: RDX `alloc` keyword — blueprint instances.
 */
static inline void *rdx_va_alloc(usize size) {
    if (size == 0) return NULL;

    /* Add 8-byte header; round up to 64 KiB (Windows allocation granularity) */
    usize total = rdx_align_up(size + 8, 65536ULL);

    void *mem = VirtualAlloc(
        NULL,
        total,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (mem == NULL) return NULL;

    /* VirtualAlloc always zero-initialises; store total in header */
    *((u64*)mem) = (u64)total;

    return (u8*)mem + 8;   /* return user pointer */
}

/*
 * rdx_va_free — releases a block allocated by rdx_va_alloc.
 *               This is the implementation of the RDX `drop` keyword.
 *               Passing NULL is a no-op.
 */
static inline void rdx_va_free(void *ptr) {
    if (ptr == NULL) return;
    void *base = (u8*)ptr - 8;
    /* dwSize must be 0 when dwFreeType == MEM_RELEASE */
    VirtualFree(base, 0, MEM_RELEASE);
}

/*
 * rdx_alloc — unified entry point mirroring the RDX `alloc` keyword.
 *             Always uses rdx_va_alloc (precise release via rdx_va_free).
 *
 *             The compiler's own allocations use rdx_alloc_bump() directly.
 */
static inline void *rdx_alloc(usize size) {
    return rdx_va_alloc(size);
}

/* ============================================================
 *  §8  File I/O Helpers
 *
 *  Used by the compiler to read .rdx source files and to write
 *  the generated .asm output file.
 * ============================================================ */

/*
 * rdx_read_file — reads the entire contents of `path` into a
 *                 bump-allocated, NUL-terminated buffer.
 *
 *  `out_len` receives the number of bytes read (not counting NUL).
 *  Returns NULL on any error (file not found, OOM, etc.).
 */
static inline char *rdx_read_file(const char *path, usize *out_len) {
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        rdx_print_err("[RDX] Cannot open file: ");
        rdx_print_err(path);
        rdx_print_err("\r\n");
        return NULL;
    }

    i64 file_size = 0;
    if (!GetFileSizeEx(hFile, &file_size)) {
        CloseHandle(hFile);
        rdx_print_err("[RDX] Cannot get file size: ");
        rdx_print_err(path);
        rdx_print_err("\r\n");
        return NULL;
    }

    if (file_size < 0 || (u64)file_size >= MAX_SOURCE_SIZE) {
        CloseHandle(hFile);
        rdx_print_err("[RDX] File too large: ");
        rdx_print_err(path);
        rdx_print_err("\r\n");
        return NULL;
    }

    usize sz  = (usize)file_size;
    char *buf = (char*)rdx_alloc_bump(sz + 1);
    if (!buf) {
        CloseHandle(hFile);
        rdx_print_err("[RDX] OOM reading file\r\n");
        return NULL;
    }

    DWORD bytes_read = 0;
    if (!ReadFile(hFile, buf, (DWORD)sz, &bytes_read, NULL)) {
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);
    buf[bytes_read] = '\0';
    if (out_len) *out_len = (usize)bytes_read;
    return buf;
}

/*
 * rdx_write_file — creates (or overwrites) a file at `path` and
 *                  writes `len` bytes from `data` into it.
 *  Returns 0 on success, -1 on failure.
 */
static inline i32 rdx_write_file(const char *path,
                                   const char *data, usize len)
{
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) {
        rdx_print_err("[RDX] Cannot create file: ");
        rdx_print_err(path);
        rdx_print_err("\r\n");
        return -1;
    }

    usize written = 0;
    while (written < len) {
        DWORD chunk = (DWORD)((len - written > 0x7FFFFFFFUL)
                              ? 0x7FFFFFFFUL : len - written);
        DWORD w = 0;
        if (!WriteFile(hFile, data + written, chunk, &w, NULL) || w == 0) {
            CloseHandle(hFile);
            return -1;
        }
        written += w;
    }

    CloseHandle(hFile);
    return 0;
}

/* §9  Compiler-Global Limits — now defined near top of file (§1b) to avoid
       forward-reference issues.  This comment kept for section numbering. */

/* ============================================================
 *  §10  Windows Entry Point
 *
 *  When compiling with `gcc -nostdlib`, MinGW does NOT link the
 *  C runtime startup code (crt0.o / crtexe.o).  The OS hands
 *  control directly to the symbol named `mainCRTStartup`.
 *
 *  We define it here (as a weak symbol so that main.c can
 *  override it if needed) and perform all one-time initialisation:
 *    1. Cache standard handles.
 *    2. Initialise the bump-allocator arena.
 *    3. Call the compiler's rdx_compiler_main() function.
 *    4. Exit cleanly.
 *
 *  NOTE:  Every .c file in the compiler should include core.h,
 *         but only ONE translation unit must define
 *         RDX_DEFINE_ENTRY before the include to get the entry
 *         point.  main.c does this.
 * ============================================================ */

#ifdef RDX_DEFINE_ENTRY

/*
 * rdx_compiler_main — forward declaration.
 *   Defined in main.c.  Receives argc / argv from the Windows
 *   command line (parsed here from the raw command line string).
 *
 *   For simplicity in V1 we do not parse argv; we use
 *   GetCommandLineA() in main.c instead.
 */
extern int rdx_compiler_main(void);

/*
 * mainCRTStartup — true Windows PE entry point (no CRT).
 */
void mainCRTStartup(void) {
    /* 1. Cache standard handles */
    rdx_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    rdx_stderr = GetStdHandle(STD_ERROR_HANDLE);
    rdx_stdin  = GetStdHandle(STD_INPUT_HANDLE);

    /* 2. Initialise bump allocator */
    rdx_heap_init();

    /* 3. Run the compiler */
    int exit_code = rdx_compiler_main();

    /* 4. Exit */
    ExitProcess((DWORD)exit_code);
}

#endif  /* RDX_DEFINE_ENTRY */

#endif // RDX_PLATFORM_WINDOWS_IMPL_H
