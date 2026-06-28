# RDX Language — Windows Architecture Document
# Principal Author: RDX Compiler Team
# Target: x86-64 Windows (Win64 ABI)
# Bootstrap: V1 via MinGW  `gcc -nostdlib -lkernel32`
#             V2 self-hosted, compiled entirely in RDX

================================================================================
  OVERVIEW
================================================================================

RDX is a compiled, statically-typed, manually-managed systems programming
language targeting 64-bit Windows.  The compiler pipeline is a classical
4-stage design:

  Source Text (.rdx)
       │
       ▼
  [LEXER]       → Token stream
       │
       ▼
  [PARSER]      → Abstract Syntax Tree (AST)
       │
       ▼
  [SEMANTIC]    → Type-checked, annotated AST  (future phase)
       │
       ▼
  [CODEGEN]     → NASM Win64 .asm file
       │
       ▼
  [ASSEMBLER]   → nasm -f win64 output.asm -o output.obj
       │
       ▼
  [LINKER]      → ld output.obj -lkernel32 -o output.exe   (MinGW ld)
                  — OR —  link.exe output.obj kernel32.lib  (MSVC link)

================================================================================
  WINDOWS VS LINUX — KEY DIFFERENCES
================================================================================

  ┌────────────────────┬──────────────────────┬──────────────────────────────┐
  │ Concern            │ Linux V1             │ Windows V2 (this doc)        │
  ├────────────────────┼──────────────────────┼──────────────────────────────┤
  │ I/O               │ SYS_WRITE syscall    │ WriteFile via kernel32.dll   │
  │ Memory            │ sys_brk / sys_mmap   │ VirtualAlloc / VirtualFree   │
  │ Entry point       │ _start               │ mainCRTStartup               │
  │ Calling convention│ System V AMD64       │ Microsoft x64 (4 reg params, │
  │                   │ (rdi,rsi,rdx,rcx…)   │  rcx,rdx,r8,r9 + shadow sp) │
  │ NASM format       │ -f elf64             │ -f win64                     │
  │ Object extension  │ .o                   │ .obj                         │
  │ Link flag         │ (none extra)         │ -lkernel32                   │
  │ Stack alignment   │ 16-byte before call  │ 16-byte + 32-byte shadow sp  │
  └────────────────────┴──────────────────────┴──────────────────────────────┘

================================================================================
  PART 1A — LEXER: Token Data Structures
================================================================================

  typedef enum {
      /* Literals */
      TOK_NUM_LIT,      /* 42, 3                                          */
      TOK_STR_LIT,      /* "hello"                                        */

      /* Keywords */
      TOK_NUM,          /* num                                            */
      TOK_STR,          /* str                                            */
      TOK_FN,           /* fn                                             */
      TOK_RETURN,       /* return                                         */
      TOK_IF,           /* if                                             */
      TOK_ELSE,         /* else                                           */
      TOK_LOOP,         /* loop                                           */
      TOK_BLUEPRINT,    /* blueprint                                      */
      TOK_ALLOC,        /* alloc                                          */
      TOK_DROP,         /* drop                                           */
      TOK_SYS,          /* sys                                            */
      TOK_CALL,         /* call                                           */

      /* Identifiers */
      TOK_IDENT,        /* user-defined names                             */

      /* Operators */
      TOK_PLUS,         /* +                                              */
      TOK_MINUS,        /* -                                              */
      TOK_STAR,         /* *                                              */
      TOK_SLASH,        /* /                                              */
      TOK_EQ,           /* ==                                             */
      TOK_NEQ,          /* !=                                             */
      TOK_LT,           /* <                                              */
      TOK_GT,           /* >                                              */
      TOK_ASSIGN,       /* :                                              */

      /* Punctuation */
      TOK_LPAREN,       /* (                                              */
      TOK_RPAREN,       /* )                                              */
      TOK_LBRACE,       /* {                                              */
      TOK_RBRACE,       /* }                                              */
      TOK_SEMICOLON,    /* ;                                              */
      TOK_COMMA,        /* ,                                              */
      TOK_ARROW,        /* ->                                             */
      TOK_DOT,          /* .                                              */

      TOK_EOF,
      TOK_UNKNOWN,
  } TokenType;

  typedef struct {
      TokenType   type;
      char        value[256];   /* raw lexeme text                        */
      int         line;         /* source line (for error messages)       */
  } Token;

  Token token_stream[MAX_TOKENS];
  int   token_count;

================================================================================
  PART 1B — PARSER: AST Node Data Structures
================================================================================

  typedef enum {
      NODE_PROGRAM,
      NODE_VAR_DECL,       /* num age : 25;                              */
      NODE_FN_DECL,        /* fn foo(x: num) -> num { ... }              */
      NODE_RETURN,         /* return expr;                                */
      NODE_IF,             /* if (cond) { ... } else { ... }             */
      NODE_LOOP,           /* loop (cond) { ... }                        */
      NODE_BLUEPRINT,      /* blueprint Player { ... }                   */
      NODE_ALLOC,          /* alloc Player()                             */
      NODE_DROP,           /* drop p;                                    */
      NODE_ASSIGN,         /* p : expr                                   */
      NODE_CALL,           /* call("hello") or foo(x)                   */
      NODE_BINARY_OP,      /* x + 1,  count < 10                         */
      NODE_LITERAL_NUM,    /* 42                                         */
      NODE_LITERAL_STR,    /* "hello"                                    */
      NODE_IDENT,          /* x, age, p                                  */
      NODE_SYS_BLOCK,      /* sys.rdx { ... }                            */
      NODE_MEMBER_ACCESS,  /* p.health                                   */
      NODE_BLOCK,          /* { stmt; stmt; }                            */
  } NodeType;

  typedef struct ASTNode {
      NodeType type;
      char     sval[256];       /* string payload (ident name, literal)  */
      long     ival;            /* integer payload (numeric literals)     */
      char     op[4];           /* operator  (+, -, *, /, ==, <, ...)    */
      char     rdx_type[32];    /* declared type ("num", "str", ...)      */

      /* Child indices into node_pool[], -1 = absent */
      int      left;
      int      right;
      int      condition;
      int      body;
      int      else_body;
      int      children[64];   /* block stmts, fn params, fields         */
      int      child_count;

      int      line;
  } ASTNode;

  ASTNode node_pool[MAX_NODES];
  int     node_count;

================================================================================
  PART 1C — CODE GENERATOR: Win64 NASM Output Strategy
================================================================================

  Key design decisions:
  ┌─────────────────────────────────────────────────────────────────────────┐
  │  Output format   : nasm -f win64                                        │
  │  Calling conv    : Microsoft x64 ABI                                    │
  │                    — first 4 integer args: rcx, rdx, r8, r9            │
  │                    — caller allocates 32-byte shadow space on stack     │
  │                    — stack 16-byte aligned BEFORE the CALL             │
  │  I/O             : GetStdHandle + WriteFile  (kernel32.dll)            │
  │  num variables   : 64-bit integers on stack (qword [rbp - offset])     │
  │  str literals    : .data section, labels _str0 / _str0_len             │
  │  Functions       : standard prologue / epilogue                        │
  │                    push rbp / mov rbp,rsp / sub rsp,N (32+locals)     │
  │  blueprints      : contiguous stack frame regions                      │
  │  alloc           : calls VirtualAlloc  (kernel32.dll)                  │
  │  drop            : calls VirtualFree   (kernel32.dll)                  │
  │  call("…")       : calls WriteFile via cached stdout HANDLE            │
  │  if / loop       : cmp + conditional jmp to _ifN_end / _loopN_start   │
  └─────────────────────────────────────────────────────────────────────────┘

  Required extern declarations at top of every generated .asm:
  ─────────────────────────────────────────────────────────────
      extern GetStdHandle
      extern WriteFile
      extern ReadFile
      extern VirtualAlloc
      extern VirtualFree
      extern ExitProcess
      extern CreateFileA
      extern CloseHandle

  Output file skeleton:
  ─────────────────────
      bits 64
      default rel

      extern GetStdHandle
      extern WriteFile
      extern VirtualAlloc
      extern VirtualFree
      extern ExitProcess

      section .data
          _str0     db  "Hello, Windows!", 0x0D, 0x0A
          _str0_len equ $ - _str0
          _stdout   dq  0      ; cached HANDLE

      section .bss
          _bytes_written  resd 1

      section .text
          global mainCRTStartup

      mainCRTStartup:
          sub     rsp, 40         ; 32-byte shadow space + 8-byte align fix
          ; cache stdout handle
          mov     rcx, -11        ; STD_OUTPUT_HANDLE
          call    GetStdHandle
          mov     [rel _stdout], rax

          call    rdx_main

          ; ExitProcess(0)
          xor     rcx, rcx
          call    ExitProcess

      rdx_main:
          push    rbp
          mov     rbp, rsp
          sub     rsp, 32         ; shadow space for callees
          ; ... generated body ...
          add     rsp, 32
          pop     rbp
          ret

================================================================================
  BUILD SEQUENCE  (Windows, VS Code + MinGW + NASM)
================================================================================

  1. Install MinGW-w64 (GCC for Windows) + NASM
  2. Compile the RDX compiler:
       gcc -nostdlib -O2 -o rdxc.exe src/main.c -lkernel32

  3. Compile an RDX source file:
       rdxc.exe hello.rdx -o hello.asm
       nasm -f win64 hello.asm -o hello.obj
       ld hello.obj -lkernel32 -o hello.exe   (MinGW ld)

  4. Run:
       hello.exe

================================================================================
  FILE LAYOUT
================================================================================

  rdx-lang/
  ├── ARCHITECTURE.md         ← this document
  ├── src/
  │   ├── core.h              ← Win32 stdlib replacement       [STEP 1]
  │   ├── test_core.c         ← smoke-test for core.h          [STEP 1]
  │   ├── lexer.h / lexer.c   ← tokeniser                      [STEP 2]
  │   ├── parser.h / parser.c ← AST builder                    [STEP 3]
  │   ├── codegen.h/codegen.c ← Win64 NASM emitter             [STEP 4]
  │   └── main.c              ← compiler entry point           [STEP 5]
  ├── examples/
  │   └── hello.rdx           ← first RDX program
  └── tests/
      └── (future unit tests)
