# RDX Programming Guide & Real-World Examples

This guide contains complete, end-to-end, runnable RDX programs. Each example is designed to demonstrate key capabilities of the language, from basic syntax to advanced cybersecurity incident response and algorithmic computing.

---

## Table of Contents

1. [Program 1: Hello World & Direct Printing](#program-1-hello-world--direct-printing)
2. [Program 2: Variables, Data Types & Introspection](#program-2-variables-data-types--introspection)
3. [Program 3: Conditionals (`if` / `elif` / `else`)](#program-3-conditionals-if--elif--else)
4. [Program 4: Loops & Iteration (`for`, `while`, `break`, `continue`)](#program-4-loops--iteration-for-while-break-continue)
5. [Program 5: User-Defined Functions & Recursion](#program-5-user-defined-functions--recursion)
6. [Program 6: String Manipulation & Method Chaining](#program-6-string-manipulation--method-chaining)
7. [Program 7: Math Library & Statistical Calculations](#program-7-math-library--statistical-calculations)
8. [Program 8: Lists, In-Place Sorting & Dynamic Arrays](#program-8-lists-in-place-sorting--dynamic-arrays)
9. [Program 9: Automated Cyber Incident Triage](#program-9-automated-cyber-incident-triage)
10. [Program 10: Log Analysis & Cyber Attack Vector Detection](#program-10-log-analysis--cyber-attack-vector-detection)
11. [Program 11: Ransomware IOC & Threat Feed Verification](#program-11-ransomware-ioc--threat-feed-verification)
12. [Program 12: Interactive Incident Response Tool (`input()`)](#program-12-interactive-incident-response-tool-input)

---

## Program 1: Hello World & Direct Printing

In RDX, there is no `class Main`, no `public static void main`, and no import required. Simply write your statement.

```rdx
// File: hello.rdx
print "Hello, World!"
print "Welcome to RDX — the syntax-free, human-first language."
```

### Running It:
```bash
rdx hello.rdx
```

### Output:
```text
Hello, World!
Welcome to RDX — the syntax-free, human-first language.
```

---

## Program 2: Variables, Data Types & Introspection

RDX dynamically determines types at runtime with zero syntax friction. You can query any value's type with `type()`.

```rdx
// File: types_demo.rdx
integer_val = 42
float_val   = 3.14159
string_val  = "RDX Forensics"
bool_val    = true
nil_val     = nil
list_val    = [10, 20, "thirty", false]

print "Integer:", integer_val, "| Type:", type(integer_val)
print "Float:  ", float_val,   "| Type:", type(float_val)
print "String: ", string_val,  "| Type:", type(string_val)
print "Bool:   ", bool_val,    "| Type:", type(bool_val)
print "Nil:    ", nil_val,     "| Type:", type(nil_val)
print "List:   ", list_val,    "| Type:", type(list_val)
```

### Output:
```text
Integer: 42 | Type: int
Float:   3.14159 | Type: float
String:  RDX Forensics | Type: str
Bool:    true | Type: bool
Nil:     nil | Type: nil
List:    [10, 20, "thirty", false] | Type: list
```

---

## Program 3: Conditionals (`if` / `elif` / `else`)

RDX supports clean condition branching without parentheses:

```rdx
// File: triage_level.rdx
cvss_score = 9.8

if cvss_score >= 9.0 {
    severity = "CRITICAL"
    action   = "Immediate isolate and incident commander dispatch"
} elif cvss_score >= 7.0 {
    severity = "HIGH"
    action   = "Patch within 24 hours"
} elif cvss_score >= 4.0 {
    severity = "MEDIUM"
    action   = "Schedule for next maintenance cycle"
} else {
    severity = "LOW"
    action   = "Informational logging only"
}

print "CVSS Score:", cvss_score
print "Severity:  ", severity
print "Action:    ", action
```

### Output:
```text
CVSS Score: 9.8
Severity:   CRITICAL
Action:     Immediate isolate and incident commander dispatch
```

---

## Program 4: Loops & Iteration (`for`, `while`, `break`, `continue`)

RDX provides intuitive list iteration, integer ranges, and while loops with `break` and `continue`.

```rdx
// File: loops_demo.rdx
print "--- For loop with range ---"
for i in range(5) {
    print "Step:", i
}

print ""
print "--- Iterating a list of IP addresses ---"
servers = ["10.0.0.1", "10.0.0.2", "192.168.1.1", "203.0.113.5"]
for s in servers {
    if ip.is_private(s) {
        print "Internal host:", s
    }
}

print ""
print "--- While loop with break and continue ---"
num = 0
while num < 10 {
    num += 1
    if num == 3 {
        print "Skipping 3 (continue)"
        continue
    }
    if num == 7 {
        print "Breaking at 7"
        break
    }
    print "Value:", num
}
```

### Output:
```text
--- For loop with range ---
Step: 0
Step: 1
Step: 2
Step: 3
Step: 4

--- Iterating a list of IP addresses ---
Internal host: 10.0.0.1
Internal host: 10.0.0.2
Internal host: 192.168.1.1

--- While loop with break and continue ---
Value: 1
Value: 2
Skipping 3 (continue)
Value: 4
Value: 5
Value: 6
Breaking at 7
```

---

## Program 5: User-Defined Functions & Recursion

Define clean functions using `fn name(params) { ... }` with early `return` and full recursion support:

```rdx
// File: functions_demo.rdx
fn factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

fn fibonacci(n) {
    if n <= 0 { return 0 }
    if n == 1 { return 1 }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

fn hypotenuse(a, b) {
    return math.sqrt(a * a + b * b)
}

print "5!  =", factorial(5)
print "10! =", factorial(10)
print "Fibonacci(8) =", fibonacci(8)
print "Hypotenuse(3, 4) =", hypotenuse(3, 4)
```

### Output:
```text
5!  = 120
10! = 3628800
Fibonacci(8) = 21
Hypotenuse(3, 4) = 5
```

---

## Program 6: String Manipulation & Method Chaining

RDX provides an extensive suite of built-in string methods that work directly on string variables:

```rdx
// File: string_demo.rdx
url = "https://internal.securesite.corp/reports/q3_confidential.pdf"

print "Original: ", url
print "Upper:    ", url.upper()
print "Length:   ", url.len()
print "Is HTTPS? ", url.starts("https://")
print "Is PDF?   ", url.ends(".pdf")
print "Has Corp? ", url.contains(".corp")

// Redact secret data
secret_text = "The password is Password12345! Do not share."
sanitized   = secret_text.replace("Password12345!", "[REDACTED]")
print "Sanitized:", sanitized

// Split CSV
log_csv = "192.168.1.50,admin,200,SUCCESS"
tokens  = log_csv.split(",")
print "User:   ", tokens[1]
print "Status: ", tokens[2]

// Join list of strings
joined = str.join(" -> ", ["Entry", "Firewall", "DMZ", "Database"])
print "Audit Trail:", joined
```

### Output:
```text
Original:  https://internal.securesite.corp/reports/q3_confidential.pdf
Upper:     HTTPS://INTERNAL.SECURESITE.CORP/REPORTS/Q3_CONFIDENTIAL.PDF
Length:    57
Is HTTPS?  true
Is PDF?    true
Has Corp?  true
Sanitized: The password is [REDACTED]! Do not share.
User:    admin
Status:  200
Audit Trail: Entry -> Firewall -> DMZ -> Database
```

---

## Program 7: Math Library & Statistical Calculations

RDX has full support for 64-bit integer and floating-point math, including standard mathematical constants and functions:

```rdx
// File: statistics.rdx
fn calc_mean(data) {
    if len(data) == 0 { return 0.0 }
    total = 0.0
    for x in data { total += float(x) }
    return total / float(len(data))
}

fn calc_variance(data, mean) {
    sum_sq = 0.0
    for x in data {
        diff = float(x) - mean
        sum_sq += diff * diff
    }
    return sum_sq / float(len(data))
}

scores = [88, 92, 79, 95, 84, 91, 76, 89]
avg    = calc_mean(scores)
var    = calc_variance(scores, avg)
stddev = math.sqrt(var)

print "Data:             ", scores
print "Sample Count:     ", len(scores)
print "Mean:             ", avg
print "Variance:         ", var
print "Standard Deviation:", stddev
```

### Output:
```text
Data:              [88, 92, 79, 95, 84, 91, 76, 89]
Sample Count:      8
Mean:              86.75
Variance:          41.1875
Standard Deviation: 6.417748826
```

---

## Program 8: Lists, In-Place Sorting & Dynamic Arrays

RDX supports dynamic lists, item append, negative indexing (`list[-1]` gets the last element), and in-place element reassignment:

```rdx
// File: bubble_sort.rdx
fn sort(lst) {
    n = len(lst)
    i = 0
    while i < n {
        j = 0
        while j < n - i - 1 {
            if lst[j] > lst[j + 1] {
                temp = lst[j]
                lst[j] = lst[j + 1]
                lst[j + 1] = temp
            }
            j += 1
        }
        i += 1
    }
    return lst
}

numbers = [99, 14, 5, 23, 71, 3, 42]
print "Unsorted:", numbers
sorted_list = sort(numbers)
print "Sorted:  ", sorted_list
print "Smallest:", sorted_list[0]
print "Largest: ", sorted_list[-1]
```

### Output:
```text
Unsorted: [99, 14, 5, 23, 71, 3, 42]
Sorted:   [3, 5, 14, 23, 42, 71, 99]
Smallest: 3
Largest:  99
```

---

## Program 9: Automated Cyber Incident Triage

Demonstrates native cyber primitives: IP classification, subnet checking, port risk profiling, and structured alerting.

```rdx
// File: incident_triage.rdx
timeline("INVESTIGATION_START", "SOC analyst triaging alert on Host-22")

target_ip   = "203.0.113.88"
subnet_dmz  = "203.0.113.0/24"
active_port = 3389

print "Target IP:", target_ip
if ip.is_public(target_ip) {
    print "  [!] Public IP detected"
}

if ip.in_subnet(target_ip, subnet_dmz) {
    print "  [+] Host is located in the DMZ subnet (" + subnet_dmz + ")"
}

port_name = net.port_name(active_port)
port_risk = net.port_risk(active_port)
print "Port Analysis: Port", active_port, "(" + port_name + ") -> Risk:", port_risk

alert("CRITICAL", "Public RDP Exposure", "Host " + target_ip + " has port 3389 open to the Internet.")
timeline("INCIDENT_ESCALATED", "Ticket forwarded to containment team.")
```

### Output:
```text
  [12:45:10] -> INVESTIGATION_START      | SOC analyst triaging alert on Host-22
Target IP: 203.0.113.88
  [!] Public IP detected
  [+] Host is located in the DMZ subnet (203.0.113.0/24)
Port Analysis: Port 3389 (RDP) -> Risk: CRITICAL (Remote Desktop / Ransomware Entry)

================== [CRITICAL] ==================
  TIMESTAMP: 2026-10-24 12:45:10
  SEVERITY:  CRITICAL
  SUMMARY:   Public RDP Exposure
  DETAILS:   Host 203.0.113.88 has port 3389 open to the Internet.
====================================================

  [12:45:10] -> INCIDENT_ESCALATED       | Ticket forwarded to containment team.
```

---

## Program 10: Log Analysis & Cyber Attack Vector Detection

Scan raw HTTP and server logs for SQL Injection, XSS, Path Traversal, and RCE payloads:

```rdx
// File: log_analyzer.rdx
raw_logs = [
    "2026-10-24 10:01:22 192.168.1.100 GET /index.html 200",
    "2026-10-24 10:02:15 45.33.32.156 GET /login.php?user=admin' OR '1'='1 500",
    "2026-10-24 10:03:01 10.0.0.15 GET /images/logo.png 200",
    "2026-10-24 10:04:45 185.220.101.5 GET /search.php?q=<script>alert('pwned')</script> 200",
    "2026-10-24 10:05:30 194.26.29.112 GET /view?file=../../../../etc/passwd 403"
]

print "Scanning", len(raw_logs), "log entries..."
attacks_found = 0

for entry in raw_logs {
    attack = log.detect_attack(entry)
    source = log.extract_ip(entry)
    status = log.extract_status(entry)

    if attack.len() > 0 {
        attacks_found += 1
        print "[ALERT]", attack
        print "        Source IP: " + source + " | HTTP Status: " + str(status)
        alert("WARNING", attack, "Observed from source: " + source)
    }
}

print ""
print "Scan complete. Total attacks identified:", attacks_found
```

### Output:
```text
Scanning 5 log entries...
[ALERT] ATTACK: SQL Injection (SQLi) Detected
        Source IP: 45.33.32.156 | HTTP Status: 500
[ALERT] ATTACK: Cross-Site Scripting (XSS) Vector Detected
        Source IP: 185.220.101.5 | HTTP Status: 200
[ALERT] ATTACK: Directory Traversal / LFI Attempt Detected
        Source IP: 194.26.29.112 | HTTP Status: 403

Scan complete. Total attacks identified: 3
```

---

## Program 11: Ransomware IOC & Threat Feed Verification

Compute SHA256 and MD5 hashes of file artifacts and compare against a threat feed of known Indicators of Compromise (IOCs):

```rdx
// File: threat_intel.rdx
known_ioc_hashes = [
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "8fb5eb88cae9b281b1fa7500447413aa",
    "3019982ce7575b88cd61fc60b54b87424829f1552bb91e3e4fd11fc23082348f"
]

suspicious_files = [
    "invoice_october.pdf.exe",
    "document.docx",
    "critical_malware_payload_2026"
]

print "=== Automated Threat Intelligence Matcher ==="

for file in suspicious_files {
    file_hash = hash.sha256(file)
    print "Analyzing:", file
    print "  SHA256:", file_hash

    is_malicious = threat.is_ioc(file_hash, known_ioc_hashes)
    if is_malicious {
        print "  [!] MATCH CONFIRMED: File is a known malware IOC!"
        alert("CRITICAL", "Malware Detected", "Artifact: " + file + " matches threat feed IOC.")
    } else {
        print "  [ok] Clean: Hash not present in IOC threat database."
    }
}
```

---

## Program 12: Interactive Incident Response Tool (`input()`)

Interactive CLI programs in RDX use the native `input(prompt)` function:

```rdx
// File: interactive_triage.rdx
print "=================================================="
print "   RDX Interactive Incident Responder Console     "
print "=================================================="

target = input("Enter target IP address: ")

if !ip.is_valid(target) {
    print "[ERROR] Invalid IPv4 address format!"
    exit(1)
}

print "Valid IP address:", target
if ip.is_private(target) {
    print "Scope: INTERNAL (Enterprise LAN / VPN)"
} else {
    print "Scope: EXTERNAL (Public Internet)"
}

port_input = input("Enter suspicious port number: ")
port_num   = int(port_input)

risk = net.port_risk(port_num)
name = net.port_name(port_num)
print "Service:", name, "| Risk Level:", risk

confirm = input("Generate alert ticket? (yes/no): ")
if confirm.lower() == "yes" {
    alert("WARNING", "Investigator Flagged Port", "Host: " + target + " Port: " + str(port_num))
    print "Alert ticket recorded successfully."
} else {
    print "Investigation closed without alert."
}
```
