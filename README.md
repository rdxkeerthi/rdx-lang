# RDX Forensics & Cyber Investigation Language (v3.0)

> **The Syntax-Free, Human-First Standalone Language for Incident Response, Threat Hunting, and Network Forensics.**

[![Standalone Native](https://img.shields.io/badge/Runtime-100%25_Standalone-brightgreen)](#standalone-architecture)
[![Syntax-Free](https://img.shields.io/badge/Syntax-Free_%26_Zero--Boilerplate-blue)](#syntax-free-philosophy)
[![Diagnostics](https://img.shields.io/badge/Diagnostics-Where_%26_Why-orange)](#the-where--why-diagnostic-engine)
[![Cyber Forensics](https://img.shields.io/badge/Domain-Cyber_Investigation-crimson)](#built-in-cyber-forensics-primitives)
[![Zero Dependencies](https://img.shields.io/badge/Dependencies-Zero-success)](#zero-dependencies)

---

## 📚 Documentation Index

| Guide | Description |
|---|---|
| 📖 **[INSTALLATION.md](file:///c:/Users/admin/Downloads/rdx-lang/INSTALLATION.md)** | Step-by-step setup for Windows, Linux & macOS with zero external dependencies |
| 💻 **[PROGRAMS.md](file:///c:/Users/admin/Downloads/rdx-lang/PROGRAMS.md)** | 12 real-world end-to-end runnable programs with terminal outputs & walkthroughs |
| ⚡ **[FEATURES.md](file:///c:/Users/admin/Downloads/rdx-lang/FEATURES.md)** | Comprehensive technical breakdown of all language features, functions & builtins |
| 🚀 **[FUTURES.md](file:///c:/Users/admin/Downloads/rdx-lang/FUTURES.md)** | Strategic architectural roadmap (v3.5 - v5.0: PCAP, Bytecode VM, Wasm, Self-hosting) |
| 💡 **[CORE_PROBLEMS_SOLVED.md](file:///c:/Users/admin/Downloads/rdx-lang/CORE_PROBLEMS_SOLVED.md)** | Analysis of dependency hell, boilerplate tax, and cryptic error messages solved by RDX |

---

## 1. Why RDX? (The Real-World Problem It Solves)

During an active cybersecurity breach or ransomware outbreak at 3:00 AM, Security Operations Center (SOC) analysts and Digital Forensics & Incident Response (DFIR) specialists need to act rapidly. They need to parse firewall logs, calculate payload hashes, verify compromised internal IP subnets, audit vulnerable ports, and record incident timelines.

### The Pain Points in Today's Languages:
1. **Python is Verbose and Fragile Under Pressure**:
   To check whether an IP is in a subnet or hash a malicious payload in Python, an analyst must import `re`, `ipaddress`, `hashlib`, `socket`, and handle numerous edge cases. A minor typo or type mismatch results in a 40-line traceback (`TypeError: '>' not supported between instances...` or `KeyError: 'src_ip'`) right in the middle of a live containment effort.
2. **Bash is Brittle and Platform-Dependent**:
   Analysts string together fragile pipelines of `grep -E`, `awk`, `sed`, and `sort | uniq -c`. These pipelines break on spaces, fail on Windows environments, and become impossible to maintain for complex investigative logic.
3. **Dependency Hell & Air-Gapped Failures**:
   Most enterprise security environments are air-gapped without internet access. Languages that depend on `pip install`, `npm install`, or JVM runtimes cannot be installed or updated during an active emergency.

### The RDX Solution:
* **Syntax-Free & Zero-Friction:** No boilerplate classes, no `fn main()`, no mandatory type declarations, and semicolons are completely optional. Code executes immediately from top to bottom.
* **Human-First "Where & Why" Diagnostics:** If you make a typo or logical mistake, RDX highlights the exact line with visual caret pointers (`^^^^`), tells you in plain English **WHERE** the issue is, **WHY** it happened, and provides an actionable **FIX** suggestion with fuzzy typo detection.
* **Native Forensic Primitives:** IP parsing, CIDR subnet matching, MD5/SHA-256 cryptographic hashing, attack signature detection (SQLi, XSS, Path Traversal, RCE), and network port exposure profiling are all native language features.
* **100% Standalone:** RDX compiles to a single native executable (`rdx.exe`) with **zero external runtime dependencies**. No Python, no Node, no Perl, and no external compiler are needed to write and execute RDX scripts.

---

## 2. Syntax-Free Philosophy

RDX gets out of your way. Write forensic and general-purpose logic as naturally as you think:

```rdx
// Simple, syntax-free investigation
case_id = "INC-2026-904"
print "Starting Investigation: " + case_id

// Native CIDR Subnet Bitmask Matching
internal_subnet = "10.0.0.0/16"
suspect_ip = "10.0.4.15"

if ip.in_subnet(suspect_ip, internal_subnet) {
    alert("CRITICAL", "Compromised internal machine detected", suspect_ip)
}

// Native Cryptographic Hashing
payload_hash = hash.sha256("malicious_ransomware_binary")
print "Computed Signature: " + payload_hash

// Automatic Exploit Signature Scanning in Logs
log_entry = "GET /admin?id=1' OR '1'='1 HTTP/1.1"
attack = log.detect_attack(log_entry)
if len(attack) > 0 {
    alert("WARNING", "Exploit Vector Confirmed", attack)
}
```

* **No boilerplate:** Write code from line 1.
* **Optional semicolons:** Use newlines or semicolons interchangeably.
* **Dynamic, safe typing:** Assign numbers, strings, lists, or booleans without boilerplate type annotations.
* **Compound conditions:** Use `&&`, `||`, `and`, or `or`.
* **String Multiplication:** `"#" * 50` creates horizontal rule lines instantly.

---

## 3. The "Where & Why" Diagnostic Engine

When writing code under pressure, cryptic compiler error messages waste valuable time. RDX includes an advanced diagnostic engine that explains errors in plain human language.

### Real Example: Variable Typo Detection

If you type `host_ipp` instead of `host_ip`:

```rdx
target_subnet = "192.168.1.0/24"
host_ip = "192.168.1.100"

if ip.in_subnet(host_ipp, target_subnet) {
    print "Host in subnet"
}
```

RDX immediately displays:

```text
╭─── [RDX DIAGNOSTIC: UNDEFINED VARIABLE] ────────────────────────────────────────
│ File:     investigation.rdx
│ Location: Line 4, Column 17
├────────────────────────────────────────────────────────────────────────
│    4 |  if ip.in_subnet(host_ipp, target_subnet) {
│      |                  ^^^^^^^^
├────────────────────────────────────────────────────────────────────────
│ WHERE: Line 4, Column 17 in 'investigation.rdx'
│ WHY:   Variable 'host_ipp' has not been defined.
│ FIX:   Did you mean 'host_ip'? Or assign it first with: host_ipp = ...
╰────────────────────────────────────────────────────────────────────────
```

---

## 4. Built-in Cyber Forensics Modules

RDX includes a battery of built-in cybersecurity primitives directly inside the executable:

### Network & IP Intelligence
```rdx
ip.is_valid("192.168.1.1")          // true
ip.is_private("10.0.0.5")           // true (RFC 1918)
ip.is_public("8.8.8.8")             // true
ip.is_loopback("127.0.0.1")         // true
ip.in_subnet("10.5.2.1", "10.5.0.0/16") // true
ip.octets("192.168.1.1")            // [192, 168, 1, 1]
```

### Cryptographic Hashing
```rdx
hash.md5("sample")                  // MD5 digest
hash.sha1("sample")                 // SHA-1 digest
hash.sha256("sample")               // SHA-256 digest
hash.file_sha256("payload.bin")     // Disk file SHA-256
hash.match(hash1, hash2)            // Constant-time comparison
```

### Port Risk Profiling
```rdx
net.port_name(445)                  // "SMB"
net.port_risk(445)                  // "CRITICAL (Lateral Movement / EternalBlue Surface)"
net.port_risk(3389)                 // "CRITICAL (Remote Desktop / Ransomware Entry)"
net.port_risk(22)                   // "MEDIUM (Brute Force Target)"
```

### Log Attack Extraction
```rdx
log.extract_ip(log_line)            // Extracts IPv4 from text
log.extract_url(log_line)           // Extracts URL from text
log.extract_status(log_line)        // Extracts HTTP status code
log.detect_attack(log_line)         // Detects SQLi, XSS, Path Traversal, RCE
```

### Incident Alerts & Timelines
```rdx
timeline("EVENT_NAME", "Event description")
alert("CRITICAL", "Summary headline", "Detailed incident findings")
```

---

## 5. Standard Programming Features (v3.0)

Beyond cybersecurity, RDX v3.0 is a complete general-purpose programming language:

### User-Defined Functions (`fn`)
```rdx
fn fibonacci(n) {
    if n <= 0 { return 0 }
    if n == 1 { return 1 }
    return fibonacci(n - 1) + fibonacci(n - 2)
}
print "Fibonacci(10) =", fibonacci(10)
```

### String Methods
```rdx
msg = "  RDX Language  "
print msg.trim().upper()             // "RDX LANGUAGE"
print msg.contains("Lang")          // true
print msg.replace("Language", "v3") // "  RDX v3  "
parts = "a,b,c".split(",")          // ["a", "b", "c"]
joined = str.join("-", parts)       // "a-b-c"
```

### Math Library
```rdx
print math.sqrt(25)                 // 5
print math.pow(2, 8)                // 256
print math.abs(-42)                 // 42
print math.min(10, 20)              // 10
print math.max(10, 20)              // 20
print math.sin(0)                   // 0
```

### Dynamic Lists & In-Place Mutation
```rdx
items = [64, 34, 25, 12, 22]
append(items, 99)
items[0] = 100
print "First:", items[0], "Last:", items[-1]
```

### Interactive Console (`input()`)
```rdx
name = input("Enter investigator name: ")
print "Welcome, Agent", name
```

---

## 6. Real-World Showcase: Cyber Incident Response

Run the complete built-in incident response demonstration:

```bash
rdx examples/cyber_incident_triage.rdx
```

Output:
```text
  [21:16:40] -> INCIDENT_START           | Security Incident #2026-049 Detected
Target Investigated: 10.0.4.52

================== [CRITICAL] ==================
  TIMESTAMP: 2026-10-24 12:35:51
  SEVERITY:  CRITICAL
  SUMMARY:   COMPROMISED INTERNAL HOST DETECTED
  DETAILS:   Internal machine 10.0.4.52 is beaconing
====================================================

  [21:16:40] -> PORT_AUDIT               | Checking open listening ports
Port 22 (SSH) -> Risk Profile: MEDIUM (Brute Force Target)
Port 445 (SMB) -> Risk Profile: CRITICAL (Lateral Movement / EternalBlue Surface)
Port 3389 (RDP) -> Risk Profile: CRITICAL (Remote Desktop / Ransomware Entry)

Computed Payload SHA-256: 44bf2a665fc1468b8591a2adcc2d9e44916a7979ac859576257293096048e46a
  [21:16:40] -> TRIAGE_COMPLETE          | Forensic evidence gathered. Containment actions recommended.
```

---

## 7. Interactive REPL

Launch `rdx` without arguments to access the interactive REPL:

```bash
rdx
```

```text
╔══════════════════════════════════════════════════════════════╗
║  RDX Language REPL  v3.0.0  [Standalone, Zero-Dependency]   ║
║  Syntax-Free · Human-First · Cyber Forensics Built-In       ║
╚══════════════════════════════════════════════════════════════╝
  Type 'help' for commands | 'exit' or 'quit' to leave

rdx> ip = "10.0.0.1"
rdx> print "Is private:", ip.is_private(ip)
Is private: true
rdx> print hash.sha256("test")
9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08
rdx> exit
Goodbye from RDX!
```

---

## 8. Building from Source

RDX builds on any platform in seconds with zero external libraries:

### Windows:
```cmd
build.bat
```

### Linux / macOS:
```bash
make
sudo make install
```

---

### License
RDX is released under the **MIT Open Source License**. Free for personal, academic, and enterprise security operations.