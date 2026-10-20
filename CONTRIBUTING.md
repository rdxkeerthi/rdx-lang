# Contributing to RDX Forensics Language

Thank you for your interest in contributing to **RDX**! We welcome contributions from developers, security researchers, DFIR analysts, and enthusiasts.

---

## Code of Conduct

We are committed to providing a welcoming, inclusive, and harassment-free experience for everyone. Please be respectful and constructive in all discussions, issues, and code reviews.

---

## How Can You Contribute?

1. **Adding Cyber & Forensics Primitives**: Implement native modules for network parsing, threat feed matching, or log heuristics in pure C.
2. **Improving Error Diagnostics**: Enhance the "Where & Why" diagnostic engine with more actionable suggestions and typo corrections.
3. **Writing Real-World Examples**: Create investigation scripts under `examples/` that model real DFIR incident triage workflows.
4. **Bug Reports & Feature Requests**: Open an issue detailing the behavior with reproducible `.rdx` code snippets.

---

## Development & Building

RDX is built as a freestanding, 100% standalone binary with zero external dependencies.

### Prerequisites

- **Windows:** MinGW-w64 GCC (`gcc`)
- **Linux / macOS:** Standard `gcc` or `clang` and `make`

### Building from Source

#### Windows:
```cmd
build.bat
```
This produces `rdx.exe` and `rdx_bin.exe`.

#### Linux / macOS:
```bash
make
```
This produces the standalone `rdx` binary.

---

## Running the Test Suite

Run the cyber forensics example scripts to verify that execution and diagnostics are functioning correctly:

```cmd
rdx examples\cyber_incident_triage.rdx
rdx examples\network_threat_hunt.rdx
```

To test the diagnostic engine against deliberate errors:
```cmd
rdx examples\error_diagnostic_demo.rdx
```

---

## Pull Request Guidelines

1. **Branch Naming**: Use descriptive branch names like `feature/pcap-parser` or `fix/cidr-overflow`.
2. **Commit Messages**: Follow standard conventional commits (e.g. `feat: add ip.octets helper`, `fix: handle empty string in log extractor`).
3. **Documentation**: Update `README.md` and `CHANGELOG.md` if your PR introduces new language features or changes behavior.
4. **Code Quality**: Ensure zero memory leaks and preserve standalone portability (no external runtime dependencies).
