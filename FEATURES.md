# RDX Language Features & Technical Capabilities

The **RDX Language (v3.0)** is an end-to-end, syntax-free, human-first programming language built specifically for rapid scripting, cyber forensics, incident response, and algorithmic computing.

---

## Feature Matrix at a Glance

| Feature Category | Description | Status |
|---|---|---|
| **Runtime Dependency** | Standalone native binary; zero external dependencies (no Python, Node, JVM) | **100% Complete** |
| **Syntax Philosophy** | Clean, syntax-free, human-first; no semicolons, no mandatory class/main boilerplate | **100% Complete** |
| **Error Diagnostics** | Rust/Clang-grade ASCII diagnostic frames with line, column, root-cause & actionable fix | **100% Complete** |
| **User Functions** | `fn name(params) { ... }` with local scope isolation, recursion, and return values | **100% Complete** |
| **Control Flow** | `if`, `elif`, `else`, `while`, `for ... in`, `break`, `continue` | **100% Complete** |
| **Operator Precedence** | Full recursive-descent precedence: unary, multiplicative, additive, relational, equality, logical | **100% Complete** |
| **String Methods** | `.upper()`, `.lower()`, `.trim()`, `.contains()`, `.starts()`, `.ends()`, `.replace()`, `.split()`, `.substr()`, `.find()`, `str.join()`, string repeat `*` | **100% Complete** |
| **Math Library** | 64-bit int/float, `math.sqrt`, `math.pow`, `math.abs`, `math.floor`, `math.ceil`, `math.round`, `math.min`, `math.max`, `math.log`, `math.sin`, `math.cos`, `math.tan` | **100% Complete** |
| **List Operations** | Dynamic capacity, `append()`, negative indexing (`list[-1]`), element reassignment (`lst[i] = v`), `len()`, `range()` | **100% Complete** |
| **Type Conversions** | `type(x)`, `int(x)`, `float(x)`, `str(x)`, `bool(x)` | **100% Complete** |
| **IP & Subnet Primitives** | IPv4 validation, private/public/loopback classification, CIDR subnet matching, octet extraction | **100% Complete** |
| **Cryptographic Primitives** | Native MD5, SHA1, SHA256 string and file hashing, constant-time hash comparison | **100% Complete** |
| **Network & Port Intelligence**| Built-in port database (HTTP, SSH, SMB, RDP, Telnet, Redis, Elastic, etc.) with risk profiling | **100% Complete** |
| **Log Attack Detection** | Native detection of SQL Injection, XSS, Directory Traversal, RCE payloads, CVEs, IPs, and URLs | **100% Complete** |
| **SOC & Incident Alerts** | Structured terminal alert cards (`alert()`) and chronological timeline markers (`timeline()`) | **100% Complete** |
| **Interactive REPL** | Zero-latency color REPL with auto-completion, `help`, `clear`, and multi-line continuation | **100% Complete** |

---

## Detailed Feature Breakdown

### 1. Zero-Dependency Standalone Runtime
Unlike languages that require megabytes of runtime virtual machines, package managers, or interpreters:
- RDX compiles down to a single **~460 KB native binary** (`rdx.exe` on Windows, `rdx` on Linux/macOS).
- Built strictly in standard ANSI C (C99/C11).
- Can be copied to an air-gapped forensic workstation, USB drive, or minimal container and executed immediately.

---

### 2. Syntax-Free Human-First Design
Traditional languages force unnecessary ceremony:
```java
// Java: 5 lines before you can print
public class Main {
    public static void main(String[] args) {
        System.out.println("Hello, World!");
    }
}
```
In RDX:
```rdx
print "Hello, World!"
```
- No semicolons `;`.
- No required parentheses in `if` or `while` statements (`if score > 80 { ... }`).
- Augmented assignment operators (`+=`, `-=`, `*=`, `/=`, `%=`).
- Flexible string multiplication (`"#" * 40`).

---

### 3. Rust-Grade Diagnostic Engine
When a syntax or runtime error occurs, RDX never prints cryptic tracebacks or raw memory offsets. It generates human-friendly visual diagnostics:

```text
╭─── [RDX DIAGNOSTIC: SYNTAX ERROR] ─────────────────────────────────────────
│ File:     examples/script.rdx
│ Location: Line 45, Column 12
├────────────────────────────────────────────────────────────────────────────
│   45 |      if count > 10 {
│      |             ^
├────────────────────────────────────────────────────────────────────────────
│ WHERE: Line 45, Column 12 in 'examples/script.rdx'
│ WHY:   Comparison between incompatible types 'str' and 'int'.
│ FIX:   Convert string to integer using int(count) before comparing.
╰────────────────────────────────────────────────────────────────────────────
```
Features:
- **Exact Line & Column Indicators**: `^` and `^^^^` pinpointing the exact offending token.
- **Root-Cause Analysis (WHY)**: Explains the exact logic or syntax failure in plain English.
- **Actionable Fix (FIX)**: Recommends the exact code edit needed to resolve the issue.
- **Fuzzy Typo Suggestions**: If you type `ip.is_valdi()`, RDX suggests `Did you mean 'ip.is_valid'?`.

---

### 4. Built-in Cyber Forensics & Incident Response

#### A. IP & Subnet Analysis
```rdx
ip = "192.168.1.100"
print ip.is_valid(ip)                  // true
print ip.is_private(ip)                // true (RFC 1918)
print ip.is_public(ip)                 // false
print ip.is_loopback("127.0.0.1")       // true
print ip.in_subnet(ip, "192.168.1.0/24") // true
print ip.octets(ip)                    // [192, 168, 1, 100]
```

#### B. Cryptographic Hashes
```rdx
print hash.md5("admin")                // 21232f297a57a5a743894a0e4a801fc3
print hash.sha1("admin")               // d033e22ae348aeb5660fc2140aec35850c4da997
print hash.sha256("admin")             // 8c6976e5b5410415bde908bd4dee15dfb167a9c873fc4bb8a81f6f2ab448a918
print hash.match(h1, h2)               // Constant-time timing-attack safe comparison
```

#### C. Network Port Risk Profiling
```rdx
print net.port_name(445)               // "SMB"
print net.port_risk(445)               // "CRITICAL (Lateral Movement / EternalBlue Surface)"
print net.port_risk(3389)              // "CRITICAL (Remote Desktop / Ransomware Entry)"
print net.port_risk(22)                // "MEDIUM (Brute Force Target)"
```

#### D. Log Parsing & Threat Extraction
```rdx
log_entry = "2026-10-24 14:22:01 45.33.32.156 GET /login.php?user=admin' OR '1'='1 500"

print log.extract_ip(log_entry)        // "45.33.32.156"
print log.extract_status(log_entry)    // 500
print log.detect_attack(log_entry)     // "ATTACK: SQL Injection (SQLi) Detected"
```

#### E. Incident Alerts & Timelines
```rdx
timeline("SCAN_START", "Network perimeter scan began")
alert("CRITICAL", "Ransomware Signature Detected", "Payload matches LockBit IOC.")
timeline("CONTAINMENT_INIT", "Host isolated from enterprise subnet.")
```

---

### 5. First-Class Functions & Local Scope Isolation
- Functions declared with `fn name(params) { ... }`.
- Scopes are properly isolated: local variable assignments inside a function do not bleed into or corrupt caller/global variables.
- Full recursion support for divide-and-conquer and graph traversal algorithms.
- Support for `return` anywhere in the function body.

```rdx
fn is_even(n) {
    return n % 2 == 0
}

fn clamp(val, min_val, max_val) {
    if val < min_val { return min_val }
    if val > max_val { return max_val }
    return val
}
```

---

### 6. Dynamic Lists & Array Operations
- Dynamic auto-resizing arrays.
- Heterogeneous elements (`[1, "two", 3.14, true]`).
- Appending elements with `append(list, value)`.
- Python-style negative indexing (`list[-1]` = last item, `list[-2]` = second-to-last).
- In-place element mutation:
  ```rdx
  items = [10, 20, 30]
  items[1] = 99
  print items  // [10, 99, 30]
  ```

---

### 7. Rich String Manipulation Suite
Methods can be called directly on string variables:
- `s.upper()`: Convert to uppercase.
- `s.lower()`: Convert to lowercase.
- `s.trim()`: Strip leading and trailing whitespace.
- `s.len()`: Return character count.
- `s.contains(needle)`: Boolean substring search.
- `s.starts(prefix)`: Prefix check.
- `s.ends(suffix)`: Suffix check.
- `s.replace(from, to)`: Substring replacement.
- `s.split(delimiter)`: Tokenize string into a dynamic list.
- `s.substr(start, count)`: Substring extraction.
- `s.find(needle)`: Return 0-indexed position or -1 if not found.
- `str.join(delim, list)`: Join a list of items into a single string.

---

### 8. Mathematical Functions
Full numeric support with standard IEEE 754 float arithmetic:
- `math.sqrt(x)`: Square root
- `math.pow(base, exp)`: Power / exponentiation
- `math.abs(x)`: Absolute value
- `math.floor(x)`: Floor rounding
- `math.ceil(x)`: Ceiling rounding
- `math.round(x)`: Nearest integer rounding
- `math.min(a, b)` / `math.max(a, b)`: Minimum / Maximum
- `math.log(x, base)`: Logarithm
- `math.sin(rad)`, `math.cos(rad)`, `math.tan(rad)`: Trigonometry

---

### 9. Interactive REPL
Launch `rdx` without arguments to access an interactive programming shell:
- Colored syntax feedback.
- Real-time variable persistence.
- Type `help` for interactive module inspection.
- Type `clear` to clear the console.
- Multi-line command continuation.
