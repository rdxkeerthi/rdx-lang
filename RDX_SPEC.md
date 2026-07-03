# RDX Language Specification — v2.0
**Status:** Draft · **Target:** Production Systems Programming  
**Backends:** x86-64, ARM64, RISC-V, WebAssembly  
**Toolchain:** Zero-dependency (no MSVCRT, no libc, Kernel32/syscall only)

---

## Preface

RDX is a statically-typed, memory-safe, bare-metal systems programming language.  
Its compiler pipeline is fully deterministic: identical inputs always produce identical outputs.  
There is no undefined behavior. Every edge case has a defined result or a compile-time diagnostic.

---

## 1. Program Structure

### 1.1 Entry Point
Every RDX program begins with the global `sys.rdx` block. There is exactly one per program.

```rdx
sys.rdx {
    // program entry — top-level statements execute sequentially
}
```

### 1.2 Modules
Each `.rdx` source file is an independent compilation unit called a **module**.  
Modules expose symbols using `pub`. All symbols are private by default.

```rdx
// math.rdx
pub fn add(a: num, b: num) -> num {
    ret a + b;
}
```

Import a module with `use`:

```rdx
use math;

sys.rdx {
    result: num = math.add(10, 20);
    call(result);
}
```

---

## 2. Variables and Constants

### 2.1 Declaration
Variables are declared with their 3-letter type keyword. All variables are **immutable by default**.

```rdx
// Immutable (default)
count: num = 42;
name:  chr = "Alice";
ratio: flt = 3.14;
flag:  blo = true;
pi:    dob = 3.141592653589793;

// Mutable — requires `mut` keyword
mut score: num = 0;
score = score + 10;    // OK — mutable
```

### 2.2 Type Inference
The compiler infers types when the right-hand side is unambiguous. The type keyword may be omitted:

```rdx
count = 42;          // inferred: num
name  = "Alice";     // inferred: chr
ratio = 3.14;        // inferred: flt
flag  = true;        // inferred: blo
```

### 2.3 Constants
Compile-time constants are declared with `const`. They must be computable at compile time:

```rdx
const MAX_RETRIES: num = 5;
const BUFFER_SIZE: num = 1024 * 64;
const APP_NAME:    chr = "rdx-server";
```

### 2.4 Data Types

| Keyword | Full Name      | Width   | Range / Notes                            |
|---------|---------------|---------|------------------------------------------|
| `num`   | Integer        | 64-bit  | –2⁶³ to 2⁶³–1. Overflow = compile error |
| `chr`   | String/Char    | UTF-8   | Immutable reference-counted slice        |
| `flt`   | Float 32       | 32-bit  | IEEE 754 single-precision                |
| `dob`   | Double 64      | 64-bit  | IEEE 754 double-precision                |
| `blo`   | Boolean        | 1 byte  | `true` or `false` only. No int coercion  |
| `byt`   | Byte           | 8-bit   | Unsigned raw byte (0–255)                |
| `ptr`   | Raw Pointer    | 64-bit  | Unsafe. Must be in an `unsafe { }` block |

#### Integer Overflow Policy
RDX has **no undefined overflow**. Arithmetic on `num` is checked at compile time for constants,
and wraps deterministically (two's complement) at runtime. Explicit checked arithmetic is available:

```rdx
result = num.checked_add(a, b);   // returns Option[num]
```

### 2.5 Compound Types

#### Structs (Blueprints)
```rdx
struct Point {
    x: num,
    y: num,
}

origin: Point = Point { x: 0, y: 0 };
```

#### Enums (Tagged Unions)
```rdx
enum Status {
    Ok,
    Err(chr),
    Loading(num),
}

state: Status = Status.Ok;
```

#### Tuples
```rdx
pair: (num, chr) = (1, "hello");
first = pair.0;
```

#### Slices (Fat Pointers)
```rdx
data: []num = [1, 2, 3, 4, 5];
len  = data.len;       // 5
item = data[2];        // 3 — bounds-checked; panic on overflow
```

---

## 3. Functions

### 3.1 Declaration

```rdx
fn greet(name: chr) -> chr {
    ret "Hello, " + name + "!";
}
```

Functions are immutable by default. Parameters are passed by value (copy for scalars,
reference-counted borrow for heap types). Mutation of a parameter requires explicit `mut`:

```rdx
fn increment(mut n: num) -> num {
    n = n + 1;
    ret n;
}
```

### 3.2 Multiple Return Values
Return a tuple to return multiple values without heap allocation:

```rdx
fn min_max(data: []num) -> (num, num) {
    mut lo = data[0];
    mut hi = data[0];
    loop i in data {
        if i < lo { lo = i; }
        if i > hi { hi = i; }
    }
    ret (lo, hi);
}

(lo, hi) = min_max([3, 1, 4, 1, 5, 9]);
```

### 3.3 Generic Functions
Generics use `[T]` syntax with optional trait bounds:

```rdx
fn max[T: Ord](a: T, b: T) -> T {
    if a > b { ret a; }
    ret b;
}
```

### 3.4 Compile-Time Functions
Prefix `comptime` to run a function entirely at compile time:

```rdx
comptime fn fib(n: num) -> num {
    if n <= 1 { ret n; }
    ret fib(n - 1) + fib(n - 2);
}

const FIB_10: num = fib(10);   // evaluated to 55 at compile time
```

### 3.5 Closures
Closures capture their environment by borrow (default) or by move (`move`):

```rdx
multiplier: fn(num) -> num = fn(x: num) -> num { ret x * 3; };
result = multiplier(7);   // 21

// Move capture — closure takes ownership of captured variables
offset: num = 100;
adder = move fn(x: num) -> num { ret x + offset; };
// `offset` is no longer accessible here
```

---

## 4. Control Flow

### 4.1 Conditionals

```rdx
if score > 90 {
    call("Grade: A");
} elif score > 75 {
    call("Grade: B");
} else {
    call("Grade: C");
}
```

`if` is an **expression** — it returns a value:

```rdx
label: chr = if score > 90 { "Pass" } else { "Fail" };
```

### 4.2 Loops

#### Infinite Loop
```rdx
mut i: num = 0;
loop {
    if i >= 10 { break; }
    call(i);
    i = i + 1;
}
```

#### Iterator Loop
```rdx
items: []chr = ["apple", "banana", "cherry"];
loop item in items {
    call(item);
}
```

#### Numeric Range Loop
```rdx
loop i in 0..10 {
    call(i);        // prints 0 through 9
}
```

#### While-style Loop
```rdx
mut done: blo = false;
loop while !done {
    // ...
    done = true;
}
```

### 4.3 Pattern Matching
`match` is exhaustive. The compiler rejects non-exhaustive matches.

```rdx
match state {
    Status.Ok         => { call("All good"); }
    Status.Err(msg)   => { call("Error: " [msg]); }
    Status.Loading(n) => { call("Loading: " [n]); }
}
```

Match on values:

```rdx
match code {
    200      => { call("OK"); }
    404      => { call("Not Found"); }
    500..599 => { call("Server Error"); }
    _        => { call("Unknown"); }
}
```

---

## 5. Error Handling

RDX has **no exceptions**. Errors are values. Every function that can fail returns a `Result[T]`.

### 5.1 The Result Type

```rdx
// Built-in generic enum
enum Result[T] {
    Ok(T),
    Err(chr),    // error is always a string in the minimal runtime
}
```

### 5.2 Returning Errors

```rdx
fn parse_port(s: chr) -> Result[num] {
    n: num = num.parse(s);
    if n < 1 || n > 65535 {
        ret Result.Err("Port out of range");
    }
    ret Result.Ok(n);
}
```

### 5.3 The `?` Propagation Operator
`?` unwraps `Ok` or **immediately returns the `Err`** from the current function.
The outer function must also return `Result[T]`.

```rdx
fn start_server(port_str: chr) -> Result[num] {
    port: num = parse_port(port_str)?;     // propagates Err automatically
    // ... bind socket ...
    ret Result.Ok(port);
}
```

### 5.4 Explicit Handling

```rdx
match parse_port("8080") {
    Result.Ok(p)    => { call("Listening on " [p]); }
    Result.Err(msg) => { call("Failed: " [msg]); }
}
```

### 5.5 The `Option` Type

```rdx
enum Option[T] {
    Some(T),
    None,
}

fn find(haystack: []chr, needle: chr) -> Option[num] {
    loop i in 0..haystack.len {
        if haystack[i] == needle { ret Option.Some(i); }
    }
    ret Option.None;
}
```

### 5.6 Panics
A `panic` is a non-recoverable error (bounds violation, arithmetic overflow in debug mode,
explicit programmer assertion). It prints a stack trace and terminates the process.

```rdx
assert(divisor != 0, "Division by zero");
panic("Unreachable state reached");
```

---

## 6. Memory and Resource Management

### 6.1 Unified Memory Model
The compiler's **Escape Analysis** and **Lifetime Analysis** automatically determine the correct
allocation strategy. Programmers do not call `malloc` or `free`.

| Strategy     | When Used                                      | Notes                        |
|-------------|------------------------------------------------|------------------------------|
| **Stack**    | Value does not escape the declaring scope      | Zero cost. Default for scalars. |
| **Heap**     | Value escapes (returned, stored in struct, etc.)| Managed by RC or bump allocator |
| **Arena**    | Annotated with `#[arena]` or short-lived scope | Freed wholesale at scope exit   |
| **Static**   | `const` or module-level globals                | Zero runtime cost               |
| **RC**       | Shared ownership (multiple references)         | Reference counted automatically |

### 6.2 Ownership and Moves

```rdx
a: chr = "hello";
b: chr = a;      // MOVE — `a` is no longer valid
call(b);         // OK
call(a);         // COMPILE ERROR: use of moved value `a`
```

### 6.3 Borrows (References)
Use `&` to borrow a value without transferring ownership:

```rdx
fn print_it(s: &chr) {
    call(s);
}

name: chr = "Alice";
print_it(&name);   // borrow — name is still valid
call(name);        // OK
```

### 6.4 Universal Resource Tracking
The compiler's **Resource Checker** enforces `Acquire → Use → Release` on all resources,
not just memory. This guarantees zero resource leaks at compile time.

```rdx
// File resource — automatically closed at end of block scope
{
    file: File = File.open("data.txt")?;
    content: chr = file.read_all()?;
    call(content);
}   // `file` is dropped and closed here — guaranteed

// The compiler errors if `file` is used after this block
```

Supported tracked resource types in the standard runtime:
- `File` — OS file handles
- `Socket` — TCP/UDP sockets
- `Mutex` — synchronization primitives
- `Thread` — OS threads
- `GpuBuffer` — GPU memory regions (future)

### 6.5 Explicit Allocation (Low-Level)
For systems code requiring precise control:

```rdx
unsafe {
    raw: ptr = alloc(1024);       // VirtualAlloc-backed
    // ... write to raw pointer ...
    drop(raw);                    // VirtualFree — compiler verifies exactly once
}
```

---

## 7. Concurrency

### 7.1 Tasks (Async/Lightweight Threads)

```rdx
task handle = spawn {
    // runs concurrently
    heavy_computation();
};
result = await handle;
```

### 7.2 Channels (Message Passing)

```rdx
ch: Channel[num] = Channel.new();

spawn {
    ch.send(42);
};

val = ch.recv();
call(val);   // 42
```

### 7.3 Data-Race Safety
The compiler rejects programs where a mutable value is accessible from multiple concurrent
tasks without a `Mutex` or `Channel`. This is checked statically, not at runtime.

```rdx
mut counter: num = 0;

spawn { counter = counter + 1; }  // COMPILE ERROR: mutable shared state
spawn { counter = counter + 1; }  // without synchronization
```

The safe pattern:

```rdx
counter: Mutex[num] = Mutex.new(0);

spawn { counter.lock(fn(mut v) { v = v + 1; }); };
spawn { counter.lock(fn(mut v) { v = v + 1; }); };
```

---

## 8. Traits and Interfaces

Traits define shared behavior. Structs implement traits explicitly.

```rdx
trait Printable {
    fn print(self: &Self);
}

struct Dog {
    name: chr,
}

impl Printable for Dog {
    fn print(self: &Self) {
        call("Dog: " [self.name]);
    }
}

fn display[T: Printable](item: &T) {
    item.print();
}
```

---

## 9. Compile-Time Features

### 9.1 Compile-Time Reflection

```rdx
comptime {
    fields = @type_fields(Point);   // returns []chr of field names
    call(fields[0]);                // "x"
}
```

### 9.2 Compile-Time Code Generation

```rdx
comptime fn make_getters(T: type) -> code {
    loop field in @type_fields(T) {
        emit fn get_{field}(self: &T) -> @field_type(T, field) {
            ret self.{field};
        }
    }
}

make_getters(Point);   // generates get_x() and get_y() at compile time
```

---

## 10. Interoperability

### 10.1 C FFI
Call C functions directly using `extern`:

```rdx
extern fn printf(fmt: ptr, ...) -> num;

unsafe {
    printf("Hello %s\n", "world");
}
```

### 10.2 Inline Assembly (x86-64)

```rdx
unsafe {
    asm {
        mov rax, 60
        xor rdi, rdi
        syscall
    }
}
```

---

## 11. The `call()` System Function

`call()` is the built-in output primitive. It prints to stdout, converting all arguments to
their string representations and concatenating them inline.

```rdx
// String literal only
call("Hello, world!");

// Interpolation — base string followed by variable list
call("Values: " [num_var, flt_var, chr_var]);

// Expression result
call(2 + 2);         // prints: 4
call(a == b);        // prints: 0 or 1
```

---

## Appendix A: Reserved Keywords

```
sys   fn    ret   mut   const   use   pub   struct  enum  impl
trait loop  if    elif  else    match break continue spawn await
alloc drop  unsafe  comptime  extern  asm  true  false  in
and   or    not   as    is    of    for   while  mod   self
```

## Appendix B: Operator Precedence (High to Low)

| Level | Operators                        |
|-------|----------------------------------|
| 1     | `()` `[]` `.` (postfix)          |
| 2     | `not` `-` (unary) `&` `*`        |
| 3     | `*` `/` `%`                      |
| 4     | `+` `-`                          |
| 5     | `<<` `>>`                        |
| 6     | `&` (bitwise)                    |
| 7     | `^` (xor)                        |
| 8     | `\|` (bitwise or)                |
| 9     | `==` `!=` `<` `>` `<=` `>=`     |
| 10    | `and`                            |
| 11    | `or`                             |
| 12    | `=` `+=` `-=` `*=` `/=`         |
