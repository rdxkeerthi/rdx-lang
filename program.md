

## Quick Start

```bat
:: 1. Build the compiler
.\build.bat

:: 2. Run a program
.\rdx_bin.exe examples\greet.rdx

:: 3. Try the self-hosted compiler
.\rdx_bin.exe rdxc.rdx

:: 4. (Optional) Register .rdx extension system-wide
.\install.bat
```

---

## Language Syntax (v2.0)

### Entry Point
Every RDX program starts with `sys.rdx { }`:
```rdx
sys.rdx {
    call("Hello, World!")
}
```

### Variables & Types

| Keyword | Meaning       | Example                  |
|---------|---------------|--------------------------|
| `num`   | integer       | `num age = 25`           |
| `chr`   | string        | `chr name = "Alice"`     |
| `flt`   | float         | `flt pi = 3.14`          |
| `blo`   | boolean       | `blo ok = true`          |
| `fix`   | constant      | `fix MAX = 100`          |
| `mut`   | mutable var   | `mut num score = 0`      |

```rdx
num  count    = 42          // immutable by default
mut  num score = 0          // can be reassigned
fix  MAX       = 100        // compile-time constant
chr  greeting  = "Hello"
blo  active    = true
```

### Output — `call()`
```rdx
call("Hello!")                          // plain string
call(score)                             // variable value
call("Score: " [score])                // string + value
call("" [a, " + ", b, " = ", a + b])  // multi-value interpolation
```

### Newlines — `nl()`
```rdx
nl()       // blank newline
newline()  // alias
```

### User Input — `get()`
```rdx
chr name = get("Enter your name: ")   // reads a string
num age  = get("Enter your age:  ")   // auto-detects number input
```

### Arithmetic & Operators
```rdx
num result = a + b        // add
num result = a - b        // subtract
num result = a * b        // multiply
num result = a / b        // divide (integer)
num result = a % b        // modulo

// Comparison  (returns 0 or 1)
a == b    a != b    a < b    a > b    a <= b    a >= b
```

### Control Flow
```rdx
// if / elif / else — no parentheses needed
if score >= 90 {
    call("Grade A")
} elif score >= 70 {
    call("Grade B")
} else {
    call("Try harder")
}
```

### Loops
```rdx
// Range loop
loop 1..11 as i {
    call(i)
}

// While-style
mut num x = 0
loop x < 10 {
    x = x + 1
}

// break and continue
loop 0..100 as n {
    if n == 42 { break }
    if mod(n, 2) == 0 { continue }
    call(n)
}
```

### Pattern Matching — `match`
```rdx
match day {
    1 -> { call("Monday") }
    2 -> { call("Tuesday") }
    _ -> { call("Other day") }   // wildcard
}
```

### Error Handling — `safe` / `fail`
```rdx
safe {
    num result = x / y    // risky operation
    call("Result: " [result])
} fail err {
    call("Error: " [err])
}
```

---

## Built-in Functions

### Math
| Function | Description |
|---|---|
| `abs(x)` | absolute value |
| `max(a, b)` | larger of two |
| `min(a, b)` | smaller of two |
| `pow(base, exp)` | integer power |
| `sqrt(x)` | integer square root |
| `clamp(v, lo, hi)` | clamp to range |
| `mod(a, b)` | modulo remainder |
| `digits(n)` | decimal digit count |
| `sign(n)` | -1, 0, or 1 |
| `even(n)` | 1 if even |
| `odd(n)` | 1 if odd |
| `randish(seed)` | deterministic hash |

### Strings
| Function | Description |
|---|---|
| `len(s)` | string length |
| `upper(s)` | uppercase |
| `lower(s)` | lowercase |
| `concat(a, b)` | join strings |
| `chr_at(s, i)` | character at index |
| `repeat(s, n)` | repeat string n times |

### Type Casting
| Function | Description |
|---|---|
| `str(x)` / `tostr(x)` | number to string |
| `num(s)` / `tonum(s)` | string to number |
| `flt(s)` / `toflt(s)` | string to float |
| `bool(x)` | cast to boolean |
| `type(x)` | returns type name |

### I/O
| Function | Description |
|---|---|
| `call(expr)` | print + newline |
| `nl()` | blank newline |
| `get("prompt")` | read user input |
| `print(s)` | output string |
| `println(s)` | output string + newline |

---

## Standard Library (`lib/`)

```bat
.\rdx_bin.exe lib\math.rdx   :: math built-ins demo
.\rdx_bin.exe lib\str.rdx    :: string built-ins demo
.\rdx_bin.exe lib\io.rdx     :: I/O built-ins guide
.\rdx_bin.exe lib\sys.rdx    :: system constants + diagnostics
```

---

## Example Programs

| File | Description |
|------|-------------|
| `examples/greet.rdx` | ⭐ Hello World — reads name + age, personalized greeting |
| `examples/fibonacci.rdx` | Fibonacci sequence + math built-ins |
| `examples/calculator.rdx` | Arithmetic operations + grade classifier |
| `examples/sorting.rdx` | Bubble sort + selection sort + statistics |
| `examples/string_ops.rdx` | String functions + FizzBuzz |
| `examples/prime.rdx` | Prime sieve, checker, and sum (continue + break) |
| `examples/pattern.rdx` | match/case arms, wildcard `_`, safe/fail demo |
| `examples/temperature.rdx` | °C / °F / K conversion tables + classifier |
| `examples/bmi_calc.rdx` | Interactive BMI calculator with ideal range |
| `examples/number_guess.rdx` | Number guessing game — 7 tries, win/lose |
| `examples/todo_app.rdx` | Task manager with ASCII progress bar |
| `examples/quiz.rdx` | Interactive math quiz — 5 questions, graded |
| `examples/interactive_calc.rdx` | Interactive arithmetic calculator |
| `rdxc.rdx` | 🚀 Self-hosted compiler (Stage 0+1 simulation) |

Run any example:
```bat
.\rdx_bin.exe examples\prime.rdx
.\rdx_bin.exe examples\pattern.rdx
```

---

## Architecture

```
Source (.rdx)
    │
    ▼
┌───────────┐   ┌──────────┐   ┌──────────────────────────────┐
│  Lexer    │──▶│  Parser  │──▶│  JIT Engine (codegen.c)      │
│  lexer.c  │   │ parser.c │   │  x86-64 machine code          │
│           │   │          │   │  VirtualAlloc + WriteConsoleA │
│ 48 tokens │   │ 16 nodes │   │  ReadConsoleA (get)           │
└───────────┘   └──────────┘   └──────────────────────────────┘
                                            │
                                            ▼
                                     Native execution
                                     (kernel32.dll only)
```

### Token Types: 48
### AST Node Types: 16
### IR Opcodes: 29
### Built-in Functions: 25+

---

## Self-Hosted Compiler (rdxc.rdx)

`rdxc.rdx` is the RDX compiler **written in RDX**. It defines the full token table, AST node types, and IR opcode table — and simulates IR emission for every language construct.

```bat
.\rdx_bin.exe rdxc.rdx
```

**Bootstrap stages:**
- ✅ Stage 0: `rdx_bin.exe` runs `rdxc.rdx` and prints IR simulation
- ⏳ Stage 1: `rdxc.exe` compiles any `.rdx` → IR text file
- ⏳ Stage 2: `rdxc.exe` compiles `rdxc.rdx` → native `rdxc.exe`

---

## Philosophy

| Principle | How RDX achieves it |
|-----------|---------------------|
| **Zero Dependencies** | Only `kernel32.dll` — no MSVCRT, no stdlib |
| **Zero UB** | All edge cases (div/0, negative sqrt, shift) defined |
| **Instant** | JIT: source → machine code → run in < 10ms |
| **Natural** | English-like: `make`, `ret`, `loop`, `get`, `safe`, `fail` |
| **Self-Hosting** | `rdxc.rdx` compiles itself (Stage 2 goal) |

---

## Build

```bat
:: Build the compiler
.\build.bat

:: Install + register .rdx extension
.\install.bat

:: Requirements: GCC (MinGW) + NASM in PATH
gcc --version
nasm --version
```

---

## Project Structure

```
rdx-lang/
├── src/
│   ├── core.h              — primitive types, kernel32 helpers
│   ├── lexer.h / lexer.c   — tokenizer (48 token types)
│   ├── parser.h / parser.c — recursive descent AST builder
│   └── codegen.c / .h      — JIT engine + 25+ built-ins
├── examples/
│   ├── greet.rdx           — interactive hello world
│   ├── fibonacci.rdx       — math & sequences
│   ├── calculator.rdx      — arithmetic operations
│   ├── sorting.rdx         — bubble sort + stats
│   ├── string_ops.rdx      — string functions
│   ├── prime.rdx           — prime sieve + checker
│   ├── pattern.rdx         — match, safe/fail demo
│   ├── temperature.rdx     — unit conversion
│   ├── bmi_calc.rdx        — interactive BMI
│   ├── number_guess.rdx    — guessing game
│   ├── todo_app.rdx        — task manager
│   ├── quiz.rdx            — interactive quiz
│   └── interactive_calc.rdx
├── lib/
│   ├── math.rdx            — math built-ins reference
│   ├── str.rdx             — string built-ins reference
│   ├── io.rdx              — I/O built-ins reference
│   └── sys.rdx             — system constants + diagnostics
├── rdxc.rdx                — self-hosted compiler (Stage 0+1)
├── rdx-icon.svg            — honeybee file icon
├── build.bat               — build script
├── install.bat             — Windows installer + PATH + registry
├── RDX_LANGUAGE_GUIDE.md   — complete language reference
├── RDX_SPEC.md             — formal grammar specification
└── README.md               — this file
```

---

*RDX — Fast as a bee, sharp as a sting.* 🐝
