# RDX Language Guide — Complete Reference v2.0

> The Honeybee Systems Language · Bare-Metal · Zero Dependencies · Natural Syntax

---

## Table of Contents
1. [Program Structure](#1-program-structure)
2. [Types & Variables](#2-types--variables)
3. [Output — call() and nl()](#3-output)
4. [Input — get()](#4-input)
5. [Arithmetic & Operators](#5-arithmetic--operators)
6. [Strings](#6-strings)
7. [Control Flow — if/elif/else](#7-control-flow)
8. [Loops](#8-loops)
9. [Pattern Matching — match](#9-pattern-matching)
10. [Error Handling — safe/fail](#10-error-handling)
11. [Math Built-ins](#11-math-built-ins)
12. [String Built-ins](#12-string-built-ins)
13. [Type Casting](#13-type-casting)
14. [Constants — fix](#14-constants)
15. [Functions — make](#15-functions)
16. [Comments](#16-comments)
17. [Standard Library](#17-standard-library)
18. [Complete Examples](#18-complete-examples)

---

## 1. Program Structure

Every RDX program is wrapped in a `sys.rdx { }` block. This is the entry point.

```rdx
sys.rdx {
    // your code goes here
    call("Hello, World!")
}
```

- One `sys.rdx` block per file
- All code executes top-to-bottom
- `//` comments are stripped before parsing

---

## 2. Types & Variables

### Type Keywords

| Keyword | Description  | Size   | Example              |
|---------|--------------|--------|----------------------|
| `num`   | integer      | 64-bit | `num x = 42`         |
| `chr`   | string       | 255ch  | `chr s = "hello"`    |
| `flt`   | float        | 64-bit | `flt f = 3.14`       |
| `blo`   | boolean      | 1-bit  | `blo ok = true`      |
| `dob`   | double       | 64-bit | `dob d = 1.23456`    |
| `byt`   | byte         | 8-bit  | `byt b = 255`        |

### Variable Declaration

```rdx
// Immutable (default)
num age = 25
chr name = "Alice"
flt ratio = 0.75

// Mutable (can be reassigned)
mut num score = 0
mut chr status = "pending"

score = score + 10    // OK — mut allows this

// Compile-time constant
fix MAX = 100
fix PI  = 3
fix APP = "RDX"
```

### Boolean Literals

```rdx
blo yes = true
blo no  = false
blo YES = TRUE     // uppercase aliases also work
blo NO  = FALSE
```

---

## 3. Output

### `call()` — Print a value
```rdx
call("Hello, World!")          // string literal
call(42)                       // number
call(name)                     // variable
call(a + b)                    // expression
```

### String Interpolation with `[ ]`
Combine strings and values in one `call()`:
```rdx
call("Name: " [name])
call("Score: " [score, " / 100"])
call("" [a, " + ", b, " = ", a + b])
call("Line " [i, " of " , total, ": ", items])
```

### `nl()` — Blank Newline
```rdx
nl()         // print a blank line
newline()    // alias
```

### `print()` / `println()`
```rdx
print("output without forced newline")
println("output with newline appended")
```

---

## 4. Input

### `get()` — Read from User

```rdx
// Read a string
chr name = get("Enter your name: ")

// Read a number (auto-detected if all digits)
num age = get("Enter your age: ")

// Read and ignore (as statement)
get("Press Enter to continue...")
```

`get()` uses `ReadConsoleA` directly — it reads a full line, strips the newline, and auto-detects if the content is numeric.

---

## 5. Arithmetic & Operators

### Arithmetic
```rdx
num a = 10
num b = 3
num sum  = a + b     // 13
num diff = a - b     // 7
num prod = a * b     // 30
num quot = a / b     // 3  (integer division)
num rem  = a % b     // 1  (modulo)
```

### Comparison (returns 1 = true, 0 = false)
```rdx
a == b    // equal
a != b    // not equal
a < b     // less than
a > b     // greater than
a <= b    // less than or equal
a >= b    // greater than or equal
```

### Logical
```rdx
and   // logical AND   (short-circuit)
or    // logical OR    (short-circuit)
not   // logical NOT
```

### Negation
```rdx
num neg = 0 - 42     // RDX negation (no unary minus yet)
```

---

## 6. Strings

```rdx
chr greeting = "Hello"
chr name     = "RDX"
chr full     = concat(greeting, ", " + name + "!")

num length = len(full)
chr up     = upper(full)
chr lo     = lower(full)
chr ch     = chr_at(full, 0)     // first character
chr bar    = repeat("=", 20)    // "====================" 
```

### Multi-value output (inline concatenation)
```rdx
call("Name: " [name, ", Age: ", age, ", Active: ", active])
```

---

## 7. Control Flow

### if / elif / else
```rdx
if score >= 90 {
    call("Grade: A")
} elif score >= 80 {
    call("Grade: B")
} elif score >= 70 {
    call("Grade: C")
} else {
    call("Grade: F")
}
```

- No parentheses around condition
- Braces `{}` are always required
- `elif` (not `else if`)

---

## 8. Loops

### Range Loop
```rdx
// loop start..end as variable
loop 1..11 as i {
    call(i)       // prints 1, 2, 3, ... 10
}
```

### While-Style Loop
```rdx
mut num x = 0
loop x < 100 {
    x = x + 1
}
```

### Infinite Loop (with break)
```rdx
mut num n = 0
loop {
    n = n + 1
    call(n)
    if n >= 5 { break }
}
```

### `break` — Exit Loop
```rdx
loop 0..1000 as i {
    if i == 42 { break }
}
```

### `continue` — Skip to Next Iteration
```rdx
loop 1..21 as n {
    if mod(n, 2) == 0 { continue }    // skip even numbers
    call(n)
}
```

---

## 9. Pattern Matching

```rdx
match value {
    1 -> {
        call("One")
    }
    2 -> {
        call("Two")
    }
    _ -> {
        call("Other: " [value])    // wildcard
    }
}
```

- `match` evaluates the subject once
- Arms are tested in order, first match wins
- `_` is the wildcard arm (always matches)
- Works with `num` and `chr` values

### String matching
```rdx
chr day = "Monday"
match day {
    "Monday"  -> { call("Start of week") }
    "Friday"  -> { call("End of week!") }
    _ -> { call("Midweek: " [day]) }
}
```

---

## 10. Error Handling

```rdx
safe {
    // risky operations go here
    num result = x / y
    call("Result: " [result])
} fail err {
    // runs only if something failed inside safe
    call("Error caught: " [err])
    call("Recovered safely!")
}
```

- `safe { }` marks a protected region
- If any error occurs inside, execution jumps to `fail`
- `err` is bound as a `chr` with the error description
- After `fail { }` completes, execution continues normally

---

## 11. Math Built-ins

```rdx
abs(x)              // absolute value: abs(-5) = 5
max(a, b)           // larger: max(3, 7) = 7
min(a, b)           // smaller: min(3, 7) = 3
pow(base, exp)      // power: pow(2, 10) = 1024
sqrt(x)             // floor sqrt: sqrt(144) = 12
clamp(v, lo, hi)    // clamp: clamp(150, 0, 100) = 100
mod(a, b)           // modulo: mod(17, 5) = 2
digits(n)           // digit count: digits(12345) = 5
sign(n)             // -1, 0, or 1
even(n)             // 1 if n % 2 == 0
odd(n)              // 1 if n % 2 != 0
randish(seed)       // deterministic hash (demo)
```

---

## 12. String Built-ins

```rdx
len(s)              // length: len("hello") = 5
upper(s)            // "hello" → "HELLO"
lower(s)            // "HELLO" → "hello"
concat(a, b)        // "foo" + "bar" → "foobar"
chr_at(s, i)        // character at index i
repeat(s, n)        // repeat("*", 5) = "*****"
```

---

## 13. Type Casting

```rdx
// Number to string
num x = 42
chr s = str(x)         // "42"
chr s = tostr(x)       // alias

// String to number
chr ns = "100"
num n = num(ns)         // 100
num n = tonum(ns)       // alias

// String to float
chr fs = "3.14"
flt f = flt(fs)         // 3.14
flt f = toflt(fs)       // alias

// Boolean cast
blo b = bool(0)         // false (0)
blo b = bool(42)        // true (1)

// Type query
type(42)                // "num"
type("hi")              // "chr"
type(true)              // "blo"
```

---

## 14. Constants

```rdx
fix PI    = 3             // integer approximation
fix E     = 2
fix MAX   = 1000000
fix APP   = "RDX"
fix DEBUG = 0             // 0 = off, 1 = on
```

Constants are immutable and cannot be reassigned. They are substituted at parse time.

---

## 15. Functions

```rdx
// Declare
make greet(chr name) {
    call("Hello, " [name, "!"])
}

// Declare with return type
make add(num a, num b) -> num {
    ret a + b
}

// Call
greet("Alice")
num sum = add(10, 20)
```

> **Note:** Full function dispatch is in development. Built-in functions are fully supported; user-defined functions run inline.

---

## 16. Comments

```rdx
// Single-line comment — everything after // is ignored

sys.rdx {
    // This is a comment
    call("Not a comment")  // inline comment
}
```

Multi-line comments are not yet supported. Use multiple `//` lines.

---

## 17. Standard Library

Located in `lib/` — runnable as demos and reference documentation.

### `lib/math.rdx`
```bat
.\rdx_bin.exe lib\math.rdx
```
Demonstrates: `abs`, `max`, `min`, `pow`, `sqrt`, `clamp`, `mod`, `digits`, `sign`, `even`, `odd`

### `lib/str.rdx`
```bat
.\rdx_bin.exe lib\str.rdx
```
Demonstrates: `len`, `upper`, `lower`, `concat`, `chr_at`, `repeat`, `str`, `tonum`, `type`

### `lib/io.rdx`
```bat
.\rdx_bin.exe lib\io.rdx
```
Demonstrates: `call`, `nl`, `get`, `print`, `println`

### `lib/sys.rdx`
```bat
.\rdx_bin.exe lib\sys.rdx
```
Demonstrates: platform constants, type system, casting, diagnostics

---

## 18. Complete Examples

### Hello World
```rdx
sys.rdx {
    call("Hello, World!")
}
```

### Interactive Greeting
```rdx
sys.rdx {
    chr name = get("What is your name? ")
    num age  = get("How old are you? ")
    nl()
    call("Hello, " [name, "!"])
    if age < 18 {
        call("You are young! Learn RDX now.")
    } else {
        call("Welcome, developer!")
    }
}
```

### FizzBuzz
```rdx
sys.rdx {
    loop 1..101 as n {
        num d3 = mod(n, 3)
        num d5 = mod(n, 5)
        if d3 == 0 {
            if d5 == 0 { call("FizzBuzz") }
            else       { call("Fizz") }
        } elif d5 == 0 {
            call("Buzz")
        } else {
            call(n)
        }
    }
}
```

### Prime Checker
```rdx
sys.rdx {
    num n = 97
    mut num is_prime = 1
    if n < 2 { is_prime = 0 }
    mut num d = 2
    loop d * d <= n {
        if mod(n, d) == 0 {
            is_prime = 0
            break
        }
        d = d + 1
    }
    if is_prime == 1 { call(n, " is prime!") }
    else             { call(n, " is not prime.") }
}
```

### Safe Division
```rdx
sys.rdx {
    num x = 100
    num y = 0
    safe {
        num result = x / y
        call("Result: " [result])
    } fail err {
        call("Division by zero handled!")
    }
}
```

### Pattern Match
```rdx
sys.rdx {
    loop 1..8 as day {
        match day {
            1 -> { call("Monday") }
            5 -> { call("Friday 🎉") }
            6 -> { call("Saturday 🎉") }
            7 -> { call("Sunday") }
            _ -> { call("Weekday " [day]) }
        }
    }
}
```

---

*RDX — Fast as a bee, sharp as a sting.* 🐝
