# RDX Forensics & Cyber Investigation Language

> **The Syntax-Free, Human-First Standalone Language for Incident Response, Threat Hunting, and Network Forensics.**

[![Standalone Native](https://img.shields.io/badge/Runtime-100%25_Standalone-brightgreen)](#standalone-architecture)
[![Syntax-Free](https://img.shields.io/badge/Syntax-Free_%26_Zero--Boilerplate-blue)](#syntax-free-philosophy)
[![Diagnostics](https://img.shields.io/badge/Diagnostics-Where_%26_Why-orange)](#the-where--why-diagnostic-engine)
[![Cyber Forensics](https://img.shields.io/badge/Domain-Cyber_Investigation-crimson)](#built-in-cyber-forensics-primitives)

---

## 1. Why RDX? (The Real-World Problem It Solves)

During an active cybersecurity breach or ransomware outbreak at 3:00 AM, Security Operations Center (SOC) analysts and Digital Forensics & Incident Response (DFIR) specialists need to act rapidly. They need to parse firewall logs, calculate payload hashes, verify compromised internal IP subnets, audit vulnerable ports, and record incident timelines.

### The Pain Points in Today's Languages:
1. **Python is Verbose and Fragile Under Pressure**:
   To check whether an IP is in a subnet or hash a malicious payload in Python, an analyst must import `re`, `ipaddress`, `hashlib`, `socket`, and handle numerous edge cases. A minor typo or type mismatch results in a 40-line traceback (`TypeError: '>' not supported between instances...` or `KeyError: 'src_ip'`) right in the middle of a live containment effort.
2. **Bash is Brittle and Platform-Dependent**:
   Analysts string together fragile pipelines of `grep -E`, `awk`, `sed`, and `sort | uniq -c`. These pipelines break on spaces, fail on Windows environments, and become impossible to maintain for complex investigative logic.
3. **SIEM Query Languages (KQL, SPL) Lack Dynamic Execution**:
   Query languages can search indexed logs in the cloud, but they cannot perform active socket checks, local file hashing, or custom automated forensic triage logic on an endpoint.

### The RDX Solution:
**RDX is uniquely purpose-built for Cyber Investigation and Network Forensics:**
* **Syntax-Free & Zero-Friction:** No boilerplate classes, no `fn main()`, no mandatory type declarations, and semicolons are completely optional. Code executes immediately from top to bottom.
* **Human-First "Where & Why" Diagnostics:** If you make a typo or logical mistake, RDX highlights the exact line with visual caret pointers (`^^^^`), tells you in plain English **WHERE** the issue is, **WHY** it happened, and provides an actionable **FIX** suggestion with fuzzy typo detection.
* **Native Forensic Primitives:** IP parsing, CIDR subnet matching, MD5/SHA-256 cryptographic hashing, attack signature detection (SQLi, XSS, Path Traversal, RCE), and network port exposure profiling are all native language features.
* **100% Standalone:** RDX compiles to a single native executable (`rdx.exe`) with **zero external runtime dependencies**. No Python, no Node, no Perl, and no external compiler are needed to write and execute RDX scripts.

---

## 2. Syntax-Free Philosophy

RDX gets out of your way. Write forensic logic as naturally as you think:

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

---

## 3. The "Where & Why" Diagnostic Engine

When writing code under pressure, cryptic compiler error messages waste valuable time. RDX includes a revolutionary diagnostic engine that explains errors in plain human language.

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

### Real Example: Function Typo Detection

If you accidentally call `hash.sha257` instead of `hash.sha256`:

```text
╭─── [RDX DIAGNOSTIC: NAME ERROR] ────────────────────────────────────────
│ File:     malware_scan.rdx
│ Location: Line 2, Column 5
├────────────────────────────────────────────────────────────────────────
│    2 |  h = hash.sha257(sample)
│      |      ^^^^^^^^^^^
├────────────────────────────────────────────────────────────────────────
│ WHERE: Line 2, Column 5 in 'malware_scan.rdx'
│ WHY:   Unknown function or procedure 'hash.sha257'.
│ FIX:   Did you mean 'hash.sha256'? Check spelling and try again.
╰────────────────────────────────────────────────────────────────────────
```

---

## 4. Built-in Cyber Forensics Primitives

All functions are built directly into the standalone RDX binary with zero external library requirements.

### IPv4 & CIDR Network Subnet Math
| Function | Description | Example |
| :--- | :--- | :--- |
| `ip.is_valid(ip)` | Validates IPv4 address format (0-255 octets) | `ip.is_valid("192.168.1.1") => true` |
| `ip.is_private(ip)` | Checks RFC 1918 private subnets (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, loopback) | `ip.is_private("10.5.0.1") => true` |
| `ip.is_public(ip)` | Checks if IP is a public Internet-routable address | `ip.is_public("8.8.8.8") => true` |
| `ip.is_loopback(ip)` | Checks if IP is 127.0.0.0/8 | `ip.is_loopback("127.0.0.1") => true` |
| `ip.in_subnet(ip, cidr)` | Fast bitmask matching of an IP against a CIDR block | `ip.in_subnet("192.168.1.45", "192.168.1.0/24") => true` |
| `ip.octets(ip)` | Returns octets as a list `[o1, o2, o3, o4]` | `ip.octets("10.0.4.1") => [10, 0, 4, 1]` |

### Cryptographic Hashes (Text & Disk Files)
| Function | Description | Example |
| :--- | :--- | :--- |
| `hash.md5(str)` | Fast pure C MD5 hash | `hash.md5("payload")` |
| `hash.sha1(str)` | Fast pure C SHA-1 hash | `hash.sha1("cert_data")` |
| `hash.sha256(str)` | Fast pure C SHA-256 hash | `hash.sha256("sample_binary")` |
| `hash.file_md5(path)` | Computes MD5 of a file on disk | `hash.file_md5("malware.bin")` |
| `hash.file_sha256(path)` | Computes SHA-256 of a file on disk | `hash.file_sha256("C:\\temp\\agent.exe")` |
| `hash.match(h1, h2)` | Case-insensitive constant-time hash comparison | `hash.match(h1, known_ioc)` |

### Network Port Risk & Attack Surface Profiler
| Function | Description | Example |
| :--- | :--- | :--- |
| `net.port_name(port)` | Maps port to service (22->SSH, 445->SMB, 3389->RDP) | `net.port_name(3389) => "RDP"` |
| `net.port_risk(port)` | Evaluates exposure risk profile (CRITICAL, HIGH, MEDIUM, LOW) | `net.port_risk(445) => "CRITICAL (Lateral Movement / EternalBlue Surface)"` |

### Forensic Log & Attack Vector Extraction
| Function | Description | Example |
| :--- | :--- | :--- |
| `log.extract_ip(line)` | Extracts IPv4 address from anywhere in a log string | `log.extract_ip("Failed password from 203.0.113.5") => "203.0.113.5"` |
| `log.extract_url(line)` | Extracts HTTP/HTTPS URL from log text | `log.extract_url("Referer: https://malicious.org/payload")` |
| `log.extract_cve(line)` | Extracts CVE identifiers (e.g., CVE-2024-38077) | `log.extract_cve("Exploiting CVE-2024-12345 in service")` |
| `log.extract_status(line)`| Extracts HTTP response codes (200, 403, 404, 500) | `log.extract_status("HTTP/1.1 500 Internal Error") => 500` |
| `log.detect_attack(line)` | Heuristic detection for SQLi, XSS, Path Traversal, and RCE | Detects `' OR '1'='1`, `<script>`, `../..`, `/bin/sh`, etc. |

### Incident Alerting & Chronological Timelines
| Function | Description |
| :--- | :--- |
| `alert(level, summary, detail)` | Outputs structured, color-coded security alerts (`[CRITICAL]`, `[ALERT]`, `[WARNING]`, `[INFO]`) with timestamps. |
| `timeline(event_name, detail)` | Emits structured chronological audit trail entries for forensics reports. |

---

## 5. Standalone Architecture

RDX is completely self-contained:
* **No External Runtime:** Does NOT require Python, Node.js, Ruby, or JVM.
* **No External Compiler:** You do not need GCC, Clang, or MSVC installed to run `.rdx` files. The `rdx.exe` binary directly tokenizes, checks, and executes your scripts.
* **Zero DLL Hell:** The binary is statically compiled for Windows (and Linux via `make`).

### Running a Script:
```cmd
rdx examples\cyber_incident_triage.rdx
```
or explicitly:
```cmd
rdx run examples\cyber_incident_triage.rdx
```

### Interactive Cyber REPL:
Launch `rdx` with no arguments to enter the live forensic shell:
```text
rdx
```
```text
+==============================================================+
|  RDX Forensics & Cyber Security Language v2.5 [Standalone]    |
|  Syntax-Free * Zero-Friction * Built-in Threat & Net Engine   |
+==============================================================+
Type 'help' for cybersecurity commands, or 'exit' to quit.

rdx> ip.is_private("10.0.0.1")
true
rdx> hash.sha256("threat_beacon")
84b2c140c8859ff1b3127dd08c90b638fae546de32130095a5f7823ce9723ec5
rdx> net.port_risk(3389)
CRITICAL (Remote Desktop / Ransomware Entry)
```

---

## 6. End-to-End Investigation Walkthrough

Here is a complete forensic investigation script in action (`examples\cyber_incident_triage.rdx`):

```rdx
timeline("INCIDENT_TRIGGER", "SOC alert received for suspicious outbound beaconing")

case_id = "INC-2026-8942"
target_subnet = "10.0.4.0/24"

print "=== RDX INCIDENT TRIAGE REPORT ==="
print "Case ID: " + case_id

// 1. Audit Suspect Connections
suspect_ips = ["10.0.4.52", "192.168.1.1", "203.0.113.88"]
for host in suspect_ips {
    if ip.in_subnet(host, target_subnet) {
        alert("CRITICAL", "COMPROMISED INTERNAL HOST DETECTED", "Internal machine " + host + " is beaconing")
    }
}

// 2. Scan Exposed Attack Surface
ports_to_check = [22, 445, 3389]
for p in ports_to_check {
    svc = net.port_name(p)
    risk = net.port_risk(p)
    print "Port " + p + " (" + svc + ") -> " + risk
}

// 3. Payload Signature Verification
captured_string = "mimikatz_dump_lsass_memory_payload"
malware_hash = hash.sha256(captured_string)
print "Computed Payload SHA-256: " + malware_hash

timeline("TRIAGE_COMPLETE", "Forensic evidence gathered. Containment actions recommended.")
```

**Output:**
```text
  [21:16:40] -> INCIDENT_TRIGGER         | SOC alert received for suspicious outbound beaconing
=== RDX INCIDENT TRIAGE REPORT ===
Case ID: INC-2026-8942

================== [CRITICAL] ==================
  TIMESTAMP: 2026-10-20 21:16:40
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

## 7. Scope & Roadmap

RDX Forensics is focused on delivering the most ergonomic, reliable, and expressive environment for security engineers:

1. **Phase 1 (Complete):**
   - [x] Syntax-free engine with optional semicolons and top-level execution.
   - [x] Human-first "Where & Why" diagnostic engine with visual carets and Levenshtein fuzzy suggestions.
   - [x] Native IP/CIDR bitmask operations.
   - [x] Pure C MD5, SHA-1, SHA-256 hashing (text and disk files).
   - [x] Port surface risk classification and log attack heuristics (SQLi, XSS, Path Traversal, RCE).
   - [x] 100% Standalone binary (`rdx.exe`) with interactive REPL.

2. **Phase 2 (Upcoming Scope):**
   - [ ] Native PCAP parser (inspecting packet headers, TCP flags, and DNS queries without Wireshark).
   - [ ] YARA rule parser integration (pattern matching file dumps in 1 line).
   - [ ] Automated STIX/TAXII Threat Intelligence feed ingestion.
   - [ ] Windows Event Log (EVTX) binary chunk reader.

---

## 8. Building from Source

To rebuild the standalone binary from source:

**On Windows:**
```cmd
build.bat
```

**On Linux / macOS:**
```bash
make
```

---

### License
RDX is released under the MIT Open Source License.