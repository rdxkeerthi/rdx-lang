# RDX — A Bare-Metal Systems Programming Language

> **Target**: x86-64 Windows · **Execution Engine**: Direct-to-Memory JIT Compilation
> **Philosophy**: Zero Dependencies · Instant Execution · Native Performance

## How RDX Works
RDX is a custom, bare-metal programming language built to eliminate the friction of modern C/C++ build pipelines. 

Typical systems languages require a complex sequence of steps to run: tokenizing, parsing, generating assembly, invoking an external assembler, linking object files, and outputting an `.exe`. RDX bypasses all of this by utilizing a minimal Just-In-Time (JIT) execution engine. 

**The Pipeline:**
1. **Lexer**: Converts raw text characters into a stream of tokens.
2. **Parser**: Transforms tokens into an Abstract Syntax Tree (AST), understanding custom data types and syntactic rules.
3. **JIT Code Generator**: Traverses the AST and emits raw x86-64 machine opcodes directly into a `VirtualAlloc` memory buffer.
4. **Execution**: The compiler casts the executable memory buffer into a C function pointer and jumps directly into it, invoking your code natively.

## How is RDX Different from Other Languages (and Why)?
RDX was built with a specific philosophy that separates it from mainstream languages:

- **Zero Dependencies**: RDX doesn't rely on the C standard library (MSVCRT), external linkers, or third-party assemblers. The entire compiler and runtime fit inside a single, tiny binary. Why? To maintain absolute control over the execution environment and drastically reduce compiler bloat.
- **Direct-to-Memory Execution**: Unlike Python or JavaScript that run on heavy virtual machines, or Rust/C++ that require long compile times, RDX compiles directly to native CPU instructions in milliseconds and executes them without leaving a trace on your hard drive. Why? Because fast iteration is critical for systems developers.
- **Simplified Syntax & Implicit Types**: RDX is designed for extreme clarity. Variables are initialized using concise 3-letter keywords that map clearly to underlying memory representations. Why? To make systems programming accessible and immediately readable.

## What Real-World Problems Does RDX Solve?
1. **Rapid Prototyping & Systems Scripting**: By providing instantaneous execution of native code, RDX serves as a high-performance "systems scripting language". It gives you the raw speed of C with the rapid feedback loop of Python.
2. **Educational Insight**: RDX demystifies how a compiler works under the hood. It exposes exactly how text is transformed into an Abstract Syntax Tree (AST) and then directly into executable CPU opcodes, making it a perfect learning tool for compiler design.
3. **Embedded Windows Development**: With its direct interface to `kernel32.dll` and lack of heavy runtimes, RDX is perfect for constrained environments where you need raw Windows API access without the bloat of standard toolchains.

---

## The RDX Language (Syntax & Data Types)

RDX uses simple 3-letter keywords for all of its core primitives:

* `num` = numbers
* `chr` = character (strings)
* `flt` = floating value
* `blo` = boolean
* `dob` = double

### Basic Example
Based on the RDX specification, all execution happens inside the global `sys.rdx` block:

```rdx
sys.rdx{
    // start of program
    num = 25 // number 
    chr = "My name" // characters 
    flt = "1.2"  // floating value 
    blo = "true" // boolean
    dob = "3.1415" // double
    
    call("answer : " [num, chr, flt, blo, dob]); // print statement 
}
```

---

## Quick Start

### 1. Build the RDX Compiler
You only need a standard GCC compiler (like MinGW-w64 on Windows) to build RDX. 
No standard library (`-nostdlib`) is required!

Run the included build script:
```powershell
.\build.bat
```
*(Or manually compile using gcc with `-nostdlib` flags).*

### 2. Run a Program
Run an RDX script instantly via the JIT engine.

```powershell
.\rdx.bat run examples\09_types.rdx
```

### Output:
```text
[rdx] Reading examples\09_types.rdx
[rdx] Lexing...
[rdx] 37 tokens
[rdx] Parsing...
[rdx] 20 AST nodes
[rdx] JIT Executing...
answer : 25My name1.2true3.14159
[rdx] Execution completed successfully.
```
