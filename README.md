# RDX — A Bare-Metal Systems Programming Language

> **Target**: x86-64 Windows · **Bootstrap**: `gcc -nostdlib -lkernel32` → self-hosting  
> **Philosophy**: Zero MSVCRT · Direct `kernel32.dll` · Manual memory (`alloc` / `drop`)

---

## Build Status — All Phases Complete ✅

| Phase | Module | Status |
|-------|--------|--------|
| 1 | `src/core.h` — Win32 stdlib replacement (kernel32, VirtualAlloc, strings) | ✅ Done |
| 2 | `src/lexer.h` + `src/lexer.c` — Tokeniser (35 token types) | ✅ Done |
| 3 | `src/parser.h` + `src/parser.c` — Recursive-descent AST builder | ✅ Done |
| 4 | `src/codegen.h` + `src/codegen.c` — Win64 NASM emitter | ✅ Done |
| 5 | `src/main.c` — Compiler entry point + 4-phase pipeline | ✅ Done |
| 6 | `examples/hello.rdx` — First RDX program | ✅ Done |

---

## Requirements

| Tool | Purpose | Get it |
|------|---------|--------|
| **MinGW-w64** | GCC for Windows | `scoop install mingw` or [winlibs.com](https://winlibs.com) |
| **NASM** | Win64 assembler | `scoop install nasm` or [nasm.us](https://nasm.us) |

---

## Quick Start

```powershell
# 1. Build the RDX compiler
build.bat

# 2. Smoke-test the core
build.bat test_core

# 3. Smoke-test the lexer
build.bat test_lexer

# 4. Compile + run hello.rdx end-to-end
build.bat hello
```

### Manual build (PowerShell)
If `gcc` is not in your path, run these commands in your PowerShell terminal:

```powershell
# 1. Add the compiler tools to the terminal's PATH
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH

# 2. Build the compiler
gcc -std=c11 -nostdlib -fno-builtin -mno-stack-arg-probe -O2 -o rdx_bin.exe src/main.c src/lexer.c src/parser.c src/codegen.c -lkernel32

# 3. Compile the RDX source into a standalone Windows executable (.exe)
.\rdx examples\hello.rdx

# 4. Run the compiled executable
.\hello.exe

### Using the `run` command
You can also compile and run an RDX file immediately (without leaving an `.exe` file on disk) using the `run` command. This is useful for quickly testing scripts:

```powershell
.\rdx run examples\hello.rdx
```

---

## RDX Language Reference

```rdx
// Variables
num age  : 25;
str name : "Arthur";

// Functions
fn add(x: num, y: num) -> num {
    return x + y;
}

// Blueprint (struct)
blueprint Player {
    num health;
    num ammo;
}

// Heap allocation / deallocation
Player p : alloc Player();
p.health  : 100;
drop p;

// Control flow
loop (age < 30) {
    age : age + 1;
}

if (age >= 30) {
    call("Thirty or older");
} else if (age >= 20) {
    call("Twenties");
} else {
    call("Younger than 20");
}

// Entry point — outputs to stdout via WriteFile
sys.rdx {
    call("Hello from RDX on Windows!");
}
```

---

## Project Layout

```
rdx-lang/
├── build.bat               ← One-command build / test / run
├── ARCHITECTURE.md         ← Compiler design document
├── src/
│   ├── core.h              ← Win32 freestanding core
│   ├── lexer.h / lexer.c   ← Tokeniser
│   ├── parser.h / parser.c ← AST builder
│   ├── codegen.h / codegen.c← Win64 NASM emitter
│   ├── main.c              ← Compiler driver
│   ├── test_core.c         ← Core smoke test
│   └── test_lexer.c        ← Lexer smoke test
└── examples/
    └── hello.rdx           ← First RDX program
```

---

## Compiler Pipeline

```
hello.rdx  →  [Lexer]  →  Tokens
           →  [Parser] →  AST (node pool)
           →  [Codegen]→  hello.asm  (Win64 NASM)
           →  nasm        hello.obj
           →  ld          hello.exe
```