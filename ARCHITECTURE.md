# RDX Architecture

This document describes how the RDX compiler and runtime are structured
today, and how new components (platform layer, AOT backend, self-hosting)
fit into that pipeline as they land. Keep this file in sync with reality —
if a box below isn't implemented yet, it's marked `(planned)`.

## High-level pipeline

```
 source (.rdx)
      │
      ▼
 ┌─────────┐     ┌────────┐     ┌────────────┐
 │  Lexer  │ ──▶ │ Parser │ ──▶ │ Type Check │
 └─────────┘     └────────┘     └────────────┘
                                       │
                                       ▼
                              ┌────────────────┐
                              │   Typed IR      │
                              └────────────────┘
                                 │           │
                     ┌───────────┘           └───────────┐
                     ▼                                    ▼
              ┌─────────────┐                     ┌───────────────┐
              │  JIT engine │                     │ AOT backend    │
              │ (existing)  │                     │  (planned)     │
              └─────────────┘                     └───────────────┘
                     │                                    │
                     ▼                                    ▼
              runs immediately                  native object file /
              in-process                         standalone executable
```

The key architectural rule: **the JIT and AOT backends both consume the
same Typed IR.** They must never implement their own parsing or type
checking — this is what guarantees a `.rdx` program behaves identically
whether it's run instantly for iteration or compiled to a native binary
for deployment. Divergence between the two paths is treated as a bug, not
a feature difference.

## Current components

### Lexer
Tokenizes `.rdx` source into the token stream consumed by the parser.

### Parser
Produces an AST from the token stream. Currently supports: `sys.rdx {}`
entry block, builtins, and basic control flow. Functions, structs, and
arrays are being added here first (see Roadmap).

### JIT engine
Executes the AST/IR directly for fast iteration — this is RDX's answer to
"scripting-speed feedback loop." Currently the only execution path.

### Runtime / syscalls
Currently implemented as direct calls into Windows' `kernel32.dll`. This
is being replaced by the platform abstraction layer below.

## Planned components

### Type checker
Sits between the parser and IR generation. Responsible for: function
signature checking, struct field access validation, array/slice bounds
checking where sizes are statically known, and enforcing that every
`Result`/`Option` value is either handled or explicitly propagated (never
silently dropped). This is where RDX's "zero UB" and "safety without a
borrow checker" guarantees are actually enforced — the type checker is the
single most safety-critical component in the whole pipeline.

### Memory model / arena allocator
Not a separate pipeline stage, but a standard-library-level construct
(`arena.create`, `arena.alloc`, `arena.reset`/`destroy`) backed by runtime
support for reserving and releasing memory regions via the platform layer.
The type checker enforces that values allocated from an arena cannot be
referenced after that arena is destroyed — this is RDX's alternative to a
full borrow checker: one scope-of-validity rule per arena, not a lifetime
graph per value.

### Platform abstraction layer (`platform/`)
```
platform/
  interface      -- declares the OS operations RDX programs can use
                     (file I/O, process control, time, memory mapping)
  windows/       -- current kernel32-based implementation, moved here
  linux/         -- (planned) syscall or libc-based implementation
  macos/         -- (future)
```
No compiler or runtime component outside `platform/<os>/` may call an
OS-specific API directly. This is what makes adding a new OS backend an
additive change instead of a cross-cutting refactor.

### C FFI
`extern "C"` declarations resolved at either JIT-load time (dynamic
linking to an existing `.so`/`.dll`) or AOT link time (static or dynamic
linking into the produced binary). Type checker enforces that only
FFI-safe types (integers, floats, pointers, C strings) cross the boundary.

### AOT backend
Consumes the same Typed IR as the JIT and lowers it to a native object
file, either via an existing backend (e.g. LLVM) or via a simpler
transpile-to-C intermediate step if that's faster to get correct first.
Whichever approach is chosen, correctness relative to the JIT's behavior
on the same input is the acceptance bar, not raw codegen quality — codegen
quality can improve iteratively once correctness is established.

### Self-hosting
Tracked separately in `docs/SELF_HOSTING.md`. The long-term goal is for
each pipeline stage (lexer → parser → type checker → IR generation) to be
rewritten in RDX itself, compiled by the current bootstrap compiler, one
stage at a time, without ever breaking the ability to build RDX from a
clean checkout.

## Design invariants (do not violate)

1. JIT and AOT share the front end (lexer, parser, type checker, IR) —
   never fork it.
2. No OS-specific API calls outside `platform/<os>/`.
3. The type checker is the only place safety guarantees are enforced —
   runtime checks are a fallback for cases that can't be proven statically
   (e.g. dynamically-sized slice bounds), not a substitute for static
   checking where it's possible.
4. Every accepted program has fully defined behavior — if a code path's
   behavior is unclear, it should be a compile error, not a runtime
   surprise.
