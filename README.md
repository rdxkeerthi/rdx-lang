# RDX

> A bare-metal, zero-UB programming language aiming to make systems and
> embedded programming safe without a garbage collector or a borrow
> checker — with the same source running instantly via JIT for iteration,
> and compiling to a native binary for shipping.

**Status:** early-stage / pre-alpha. Core language features (functions,
structs, arrays, memory model) are under active development — see
[Roadmap](#roadmap) below for exactly what works today.

## Why RDX

Most languages ask you to pick two of "fast," "safe," and "simple":

- **C** is fast and simple, but memory-unsafe — the majority of firmware
  and driver CVEs trace back to buffer overflows, use-after-free, and null
  derefs that a compiler could have caught.
- **Rust** is fast and safe, but its learning curve (the borrow checker,
  lifetimes) is real and keeps people away from systems programming who
  would otherwise try it.
- **Python/JS** are simple and fast to iterate in, but you rewrite in
  another language entirely once you need to ship something fast.

RDX's bet: arena/region-based memory management can deliver most of the
safety benefit developers actually need, with a fraction of the conceptual
overhead — one rule ("everything in this arena dies when the arena dies")
instead of a lifetime graph. And a single source file should be able to run
under a JIT for fast iteration *and* compile to a native binary for
deployment, with no rewrite.

## Who this is for

- Firmware/embedded developers currently stuck with C who want compile-time
  safety without adopting Rust's full complexity.
- Developers learning systems programming who've bounced off Rust's borrow
  checker.
- Anyone who prototypes in a scripting language and currently has to
  rewrite in a compiled language to ship — RDX aims to remove that step.

## Current features

- [x] Lexer and parser
- [x] JIT execution engine
- [x] Windows syscalls (via kernel32)
- [x] User-defined functions
- [x] Structs / records
- [x] Arrays and slices
- [ ] Arena-based memory model
- [ ] `Result`/`Option`-based error handling
- [ ] Modules / multi-file programs
- [x] Linux/macOS support (in progress)
- [ ] C FFI
- [x] Native (AOT) compilation
- [x] Full self-hosting (stage 2 lexer complete)

See [RDX_SPEC.md](RDX_SPEC.md) for the full language specification and
[ARCHITECTURE.md](ARCHITECTURE.md) for how the compiler/runtime is structured.

## Getting started

```bash
git clone https://github.com/rdxkeerthi/rdx-lang.git
cd rdx-lang
# build instructions — fill in once the build system is finalized
```

### Hello world

```rdx
sys.rdx {
    print("Hello, RDX!")
}
```

Run it:

```bash
rdxc run hello.rdx
```

## Roadmap

RDX is being built toward three concrete goals, in priority order. Track
detailed progress in [ROADMAP.md](ROADMAP.md).

1. **Language core** — functions, structs, arrays, an arena memory model,
   and structured error handling. Nothing else matters until real programs
   can be written.
2. **Cross-platform runtime** — a platform abstraction layer with Linux and
   Windows backends, plus C FFI so RDX is useful immediately alongside
   existing C libraries.
3. **Native compilation + self-hosting** — an AOT backend sharing the JIT's
   front end, and rewriting the compiler in RDX itself.

## Contributing

Contributions are very welcome, especially while the language core is
being built out. Please read [CONTRIBUTING.md](CONTRIBUTING.md) first —
it covers how to build the project, run tests, and the PR process. Issues
labeled `good first issue` are a good place to start.

## License

[Choose and add a license — MIT or Apache-2.0 recommended for language
tooling, see CONTRIBUTING.md for rationale.]