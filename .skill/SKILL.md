---
name: rdx-lang-development
description: Use this skill whenever working on the RDX programming language compiler/runtime (rdx-lang repo) — designing syntax, implementing the parser/lexer/codegen, adding language features (functions, structs, arrays, memory model, error handling), porting the runtime to a new OS, adding C FFI, or building tooling (syntax highlighting, LSP, package manager). Also use when writing or updating RDX's README, ARCH.md, or language specification docs.
---

# RDX Language Development Skill

## Mission statement (keep this in view for every change)

RDX exists to solve three concrete, real-world problems. Every feature added
must serve at least one of these. If a proposed feature doesn't move one of
these forward, it's scope creep — flag it and ask before building it.

1. **Embedded/low-level safety** — give firmware/driver developers something
   as fast and close-to-metal as C, but which catches null derefs, buffer
   overflows, and undefined behavior at *compile time*, not at 3am in
   production.
2. **Beginner-friendly systems programming** — deliver roughly 70% of Rust's
   safety guarantees with 20% of its conceptual overhead. No borrow checker.
   Prefer arena/region-based allocation with clear, teachable rules over
   lifetime annotations.
3. **Fast scripting with a native escape hatch** — the JIT/interpreter path
   is for fast iteration (like Python); the same source should compile to a
   native binary with no rewrite (unlike the typical Python → Rust rewrite
   pain point).
4. **Low ceremony to write** — a beginner should be able to write a working
   RDX program with minimal boilerplate and few required keywords. Every
   piece of required syntax must earn its place; if it can be inferred or
   defaulted, it should be.
5. **Compiler errors that teach, not just fail** — every error must report
   an exact line/column, point at the offending token with a caret, name
   what was expected vs. found in plain language, and — where the fix is
   unambiguous — suggest it. This is a top-tier feature, not a nice-to-have
   bolted on later; the diagnostics engine should be designed alongside the
   parser and type checker, not retrofitted.
6. **Object-oriented structuring** — classes/objects with fields and
   methods, encapsulation, and either single inheritance or interfaces
   (favor interfaces/traits for composition over deep inheritance chains,
   but the choice should be documented and deliberate, not accidental).

## Non-negotiable engineering principles

- **The bootstrap compiler is written in C, and only C.** Do not introduce
  Python, JavaScript, Go, Rust, or any other language into the compiler,
  its build system, or its tooling — ever — without the maintainer
  explicitly approving it first. If any such files already exist in the
  repo, treat porting them to C as urgent priority-zero cleanup, not
  something to work around. Once RDX is self-hosting, the only two
  languages that should ever appear in this repo are C (legacy bootstrap)
  and RDX itself.
- **Zero UB is the brand.** Any new syntax or builtin must have well-defined
  behavior in every case, including error paths. If a case is currently
  undefined, that's a bug to fix, not a doc footnote.
- **Cross-platform by construction.** Never call a Windows-only API
  (kernel32, etc.) directly from language-level code paths. All OS
  interaction must go through a `platform/` abstraction layer with a
  backend per OS (Windows, Linux first, then macOS). If you only have time
  to implement one backend, still write the abstraction boundary so a
  second backend is a pure-addition, not a refactor.
- **Self-hosting is the credibility bar.** Prefer implementing new compiler
  features in RDX itself once the language is capable enough, rather than
  only ever extending the bootstrap compiler. Track this explicitly in
  `docs/SELF_HOSTING.md`.
- **Small, orthogonal core; batteries in the standard library.** Keep the
  language core minimal (functions, structs, arrays/slices, basic control
  flow, a memory model, error handling). Push convenience features (string
  formatting, collections, I/O helpers) into a standard library written in
  RDX itself wherever possible.

## Feature priority order (do not reorder without discussion)

1. Functions with parameters, return values, and recursion.
2. A diagnostics engine (line/col tracking through the lexer and parser,
   caret-pointed error rendering) — build this alongside step 1, not after.
   Every subsequent feature must plug into it, not print its own ad hoc
   error strings.
3. Classes/objects: fields, methods, constructors, encapsulation
   (public/private), and interfaces or single inheritance for code reuse.
4. Structs / records for plain data (still useful even in an OOP language
   for lightweight, methodless data shapes — don't force everything into a
   class).
5. Arrays and slices; strings as a first-class sliced-byte type.
6. A memory model: start with arena/region allocators exposed as an
   explicit, teachable API (`arena.alloc()`, `arena.reset()`), not implicit
   GC and not a borrow checker. Document stack vs. arena allocation
   explicitly (see Documentation requirements below).
7. Error handling via an explicit `Result<T, E>` / `Option<T>`-style value
   type with a lightweight propagation operator — not exceptions.
8. Modules/imports for multi-file programs.
9. Platform abstraction layer + Linux backend (in addition to existing
   Windows backend), including how RDX programs are loaded and run as OS
   processes (entry point, exit codes, argv/env access).
10. C FFI (`extern "C"` style declarations, calling into existing `.so`/`.dll`).
11. Native code generation path (AOT) alongside the existing JIT, sharing
    the same front end (lexer/parser/typechecker) so scripting and native
    modes never diverge in semantics.
12. Stage 2 self-hosting: compile `rdxc` with `rdxc`.
13. Tooling: TextMate/tree-sitter grammar, VS Code extension, basic LSP
    (diagnostics + go-to-definition before autocomplete).

## Documentation requirements (non-negotiable)

Every one of the following must be explained in `docs/SPEC.md` or a
dedicated doc, in plain language with a diagram or worked example — not
just "left as an exercise" or assumed obvious:

- **How a program is run as an OS process**: what happens from
  `rdxc run file.rdx` to a running process — entry point resolution,
  process creation, exit codes, how the OS is asked to start/stop it.
- **How arrays and slices work**: memory layout, bounds checking (compile
  time vs. runtime, and which applies when), how they differ from a class.
- **How the stack and the arena/heap relate**: what lives on the call
  stack (local variables, function frames) vs. what must be arena-allocated
  (anything that outlives its creating function, anything dynamically
  sized), and what happens when an arena is destroyed while something still
  references it (this must be a compile error, not a crash).
- **How classes/objects are laid out in memory** and how method calls are
  dispatched (static dispatch by default; document clearly if/where dynamic
  dispatch via interfaces is used and its cost).

## Working conventions

- **Every new language feature needs**: a spec section in
  `docs/SPEC.md`, at least one example program in `examples/`, and a test
  in the compiler's test suite before it's considered done.
- **Every new builtin/syscall wrapper needs**: cross-platform behavior
  documented, and a note on what happens on the OS(es) it hasn't been
  ported to yet (compile error, not silent no-op).
- **Never silently introduce undefined behavior** to hit a deadline — land
  a smaller, fully-defined version of the feature instead.
- **Document breaking changes** in `CHANGELOG.md` and bump the version
  according to semver once RDX reaches v0.1.0.
- When in doubt between "more powerful" and "more learnable," choose
  learnable — that's RDX's differentiation from Rust/C++, not raw power.

## Files this skill expects to exist and stay current

- `docs/SPEC.md` — full language specification.
- `docs/ARCH.md` — compiler/runtime architecture (pipeline stages, JIT vs
  AOT paths, platform layer).
- `docs/SELF_HOSTING.md` — current self-hosting stage and what's blocking
  the next stage.
- `CONTRIBUTING.md` — build/test instructions and PR process.
- `examples/` — one working `.rdx` program per language feature.