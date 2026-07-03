# The RDX Language Guide

RDX is a secure-by-design, dependency-free systems programming language that compiles directly to raw x86-64 NASM Assembly. It enforces memory safety at compile-time without relying on a Garbage Collector.

## Basic Syntax

RDX uses a C-like syntax but with modernized variable declarations and a simpler keyword set.

### Variables & Data Types
Variables are declared as `type name : expression;`.
Currently supported primitive types are `num` (signed 64-bit integer) and `str` (string literal).

```rdx
num health : 100;
str name : "Hero";

// Re-assignment
health : 120;
```

### Control Flow
Conditional logic is done via `if / else`.
Loops are infinite by default but condition-bound using `loop (condition)`.

```rdx
if (health > 0) {
    // Player is alive
} else {
    // Player is dead
}

num counter : 5;
loop (counter > 0) {
    counter : counter - 1;
}
```

### Functions
Functions are declared using the `fn` keyword. Parameters and return types use the `:` and `->` operators.

```rdx
fn add_score(current: num, amount: num) -> num {
    num total : current + amount;
    return total;
}
```

### System Calls
Because RDX runs without standard libraries (`<stdio.h>`, `<stdlib.h>`), we use a specialized `sys.rdx` block to interact with the OS (currently Windows kernel32).

```rdx
sys.rdx {
    call("Hello World!\r\n");
}
```

---

## Data Structures and The Memory Process

RDX's most powerful feature is its **Static Ownership Model**. It enforces single-ownership semantics at compile-time to guarantee memory safety (no use-after-free, no double-free).

### `blueprint` (Structs)
A `blueprint` defines a custom memory layout.

```rdx
blueprint Enemy {
    num hp;
    num damage;
}
```

### 1. Allocation (`alloc`)
You dynamically allocate memory on the heap using `alloc`. This provisions the exact bytes and returns a typed reference. The variable now **owns** this memory (State: `OWN_VALID`).

```rdx
Enemy goblin : alloc Enemy();
goblin.hp : 50;
```

### 2. Move Semantics (`:`)
In RDX, memory cannot have multiple mutable owners. When you assign an object reference to a new variable, **ownership is moved**.
The old variable is marked as `OWN_MOVED`, and the compiler will physically block you from accessing it again.

```rdx
Enemy active_target : goblin;
// goblin is now MOVED.
// goblin.hp : 0; // COMPILER ERROR: Use of already moved value
```

### 3. Deallocation (`drop`)
Because there is no Garbage Collector, memory must be freed manually using the `drop` keyword. 
When dropped, the variable's state becomes `OWN_DROPPED`. 

```rdx
drop active_target;
// active_target is now DROPPED.
// drop active_target; // COMPILER ERROR: Double free detected
// active_target.hp : 0; // COMPILER ERROR: Use after free
```

### The Semantic Ownership Analyzer
This tracking happens internally during **Phase 3b** of compilation. The `SemanticAnalyzer` traverses the Abstract Syntax Tree (AST), keeping a state table of every variable. If any state transition rule is violated, compilation is immediately aborted before any assembly code is generated. This is what guarantees RDX's zero-overhead safety.
