# Changelog

All notable changes to the RDX language and runtime will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [2.5.0-forensics] - 2026-10-20

### Added
- **Syntax-Free Execution Model**:
  - Direct top-level execution without boilerplate `fn main()` or `sys.rdx { }` blocks.
  - Semicolons are 100% optional (lines terminated by newline or semicolon).
  - Dynamic, clean variable assignment (`target = "10.0.0.1"`).
  - Support for compound logical operators (`&&`, `||`, `and`, `or`).
- **Human-First "Where & Why" Diagnostic Engine**:
  - Structured ASCII border diagnostics with exact file, line, and column tracking.
  - Visual code excerpt with caret (`^^^^`) pointer to the exact token causing errors.
  - Plain-English **WHERE** location indicator.
  - Plain-English **WHY** explanation eliminating cryptic compiler errors.
  - Concrete **FIX** recommendations with Levenshtein-distance fuzzy typo suggestions for variables and built-in functions.
- **Built-in Cyber Forensics & Incident Response Engine**:
  - `ip`: Native IPv4 validation (`ip.is_valid`), RFC 1918 private detection (`ip.is_private`), public routable check (`ip.is_public`), loopback check (`ip.is_loopback`), and CIDR subnet bitmask matching (`ip.in_subnet`).
  - `hash`: Pure C cryptographic hashing for strings and disk files with MD5, SHA-1, and SHA-256 (`hash.md5`, `hash.sha1`, `hash.sha256`, `hash.file_md5`, `hash.file_sha256`, `hash.match`).
  - `net`: Port service lookup (`net.port_name`) and attack surface vulnerability classification (`net.port_risk`).
  - `log`: Forensic pattern extraction (`log.extract_ip`, `log.extract_url`, `log.extract_cve`, `log.extract_status`) and heuristic attack detection for SQL Injection, XSS, Path Traversal, and RCE (`log.detect_attack`).
  - `alert`: Structured incident alerting with color tags and timestamps (`alert`).
  - `timeline`: Chronological incident sequence audit logger (`timeline`).
- **Interactive Cyber REPL**:
  - Launch `rdx` without arguments to enter an interactive cybersecurity shell.
- **100% Standalone Runtime**:
  - Self-contained binary `rdx.exe` / `rdx` without external runtime dependencies (no Python, Node, LLVM, or GCC needed to run user scripts).

### Changed
- Standardized `build.bat` on Windows and `Makefile` on Linux for clean standalone builds.
- Refactored `main.c` CLI with `--help`, `--version`, `run`, and interactive `repl` modes.
- Overhauled `README.md` to document the cyber investigation domain, syntax-free philosophy, and API reference.

### Removed
- Cleaned up obsolete scratch test files, temporary text logs, and outdated build artifacts.
