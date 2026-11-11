# Tamizhi Language Syntax & Reference Manual (v2.3.0)

This document provides a comprehensive specification and reference for all syntax rules, constructs, keywords, and practical usages in the Tamizhi programming language.

---

## Table of Contents

1. [Introduction & Design Philosophy](#1-introduction--design-philosophy)
2. [Dual Keywords & Alias Mapping](#2-dual-keywords--alias-mapping)
3. [Comments & Program Structure](#3-comments--program-structure)
4. [Variables, Constants & Scope](#4-variables-constants--scope)
5. [Type System](#5-type-system)
   - [Primitive Types](#primitive-types)
   - [Unsigned & Fixed-Width Types](#unsigned--fixed-width-types)
   - [Pointers & References](#pointers--references)
   - [Arrays & Slices](#arrays--slices)
   - [Type Aliases (`typedef`)](#type-aliases-typedef)
6. [Functions & Methods](#6-functions--methods)
   - [Function Definitions](#function-definitions)
   - [Prototypes & Forward Declarations](#prototypes--forward-declarations)
   - [Default Parameter Values](#default-parameter-values)
7. [Operators & Expressions](#7-operators--expressions)
   - [Arithmetic, Logical, Bitwise & Compound](#arithmetic-logical-bitwise--compound)
   - [Ternary Operator (`? :`)](#ternary-operator--)
   - [Null-Coalescing Operator (`??`)](#null-coalescing-operator-)
   - [Type Casting (`(T)`, `cast<T>`, `mathu<T>`)](#type-casting-t-castt-mathut)
   - [Memory Size (`sizeof` / `alavu`)](#memory-size-sizeof--alavu)
8. [String Handling & Interpolation](#8-string-handling--interpolation)
   - [String Interpolation (`f"..."`)](#string-interpolation-f)
   - [String Concatenation (`+`)](#string-concatenation-)
   - [String Intrinsics & Method Calls](#string-intrinsics--method-calls)
9. [Control Flow](#9-control-flow)
   - [Conditionals (`if`, `else if`, `else`)](#conditionals-if-else-if-else)
   - [Loops (`while`, `for`, `do-while`)](#loops-while-for-do-while)
   - [Integer Ranges (`0..10`, `1..=5`) & Range-Based Loops](#integer-ranges-010-15--range-based-loops)
   - [Branching (`break`, `continue`, `return`)](#branching-break-continue-return)
   - [Switch Statements (`switch`, `case`, `default`)](#switch-statements-switch-case-default)
   - [Scope-Exit Cleanup (`defer` / `pinbu`)](#scope-exit-cleanup-defer--pinbu)
10. [Object-Oriented Programming (OOP)](#10-object-oriented-programming-oop)
    - [Class Declarations](#class-declarations)
    - [Constructors (`init`, `uruvakku`)](#constructors-init-uruvakku)
    - [Inheritance & Base Calls (`super` / `thai`)](#inheritance--base-calls-super--thai)
    - [Heap Memory (`new` / `delete`)](#heap-memory-new--delete)
11. [Composite Data Types](#11-composite-data-types)
    - [Structs (`struct` / `kattamaipu`)](#structs-struct--kattamaipu)
    - [Enums (`enum` / `enumaripu`)](#enums-enum--enumaripu)
    - [Unions (`union` / `ondrippu`)](#unions-union--ondrippu)
12. [Built-in Intrinsics](#12-built-in-intrinsics)
    - [Console Output (`pathi`, `print`, `println`)](#console-output-pathi-print-println)
    - [Math Intrinsics](#math-intrinsics)
    - [File I/O Intrinsics](#file-io-intrinsics)
13. [Modules, Headers & C FFI](#13-modules-headers--c-ffi)
    - [Tamizhi Headers (`.tzh`) & `serku` / `import`](#tamizhi-headers-tzh--serku--import)
    - [C Foreign Function Interface (`extern` / `veli`)](#c-foreign-function-interface-extern--veli)
14. [Compiler Options & CLI Reference](#14-compiler-options--cli-reference)

---

## 1. Introduction & Design Philosophy

Tamizhi is a modern compiled language that combines the expressive vocabulary of **Tanglish** (Tamil words transliterated in English script) with standard **C/English** keywords. 

Key principles:
- **Zero Overhead**: Translates directly to optimized C99, compiled with GCC or Clang.
- **Bilingual Keywords**: Use pure Tanglish, standard C syntax, or seamlessly intermix both.
- **Memory Options**: Compiles with either high-performance Boehm Garbage Collector (`--gc`) or manual heap allocation (`--no-gc`).
- **Modern Ergonomics**: First-class string interpolation, defer cleanup, range loops, null coalescing, and Clang-grade compiler diagnostics.

---

## 2. Dual Keywords & Alias Mapping

Every statement or declaration can be expressed with either Tanglish or English keywords:

| Construct | Tanglish Keyword | English / C Keyword | Purpose |
|---|---|---|---|
| Variable Declaration | `edu` | `var`, `let` | Declares local or global variable |
| Function Declaration | `seyal` | `fn`, `func`, `function` | Defines or prototypes a function |
| Return Statement | `kudu` | `return` | Returns value from function |
| Print Output | `pathi(...)` | `print(...)`, `println(...)` | Console output |
| If Condition | `@(cond) aga irundha` | `if (cond)` | Branch if condition is true |
| Else-If Condition | `^(cond) aga irundha` | `else if (cond)` | Alternate condition branch |
| Else Branch | `illana` | `else` | Fallback branch |
| While Loop | `sutri (cond)` | `while (cond)` | Loop while condition is true |
| For Loop | `padigal (...)` | `for (...)` | 3-part or range-based for loop |
| Range Iteration | `ul` | `in` | Iteration operator |
| Do-While Loop | `sei { ... } sutri (...)` | `do { ... } while (...)` | Loop executed at least once |
| Switch Statement | `therivu (expr)` | `switch (expr)` | Multi-branch value matching |
| Case Label | `nilai val:` | `case val:` | Match arm inside a switch |
| Default Label | `muthal:` | `default:` | Default switch arm |
| Break Statement | `udai;` | `break;` | Exits innermost loop/switch |
| Continue Statement | `thodar;` | `continue;` | Advances to next iteration |
| Scope-Exit Cleanup | `pinbu <stmt>;` | `defer <stmt>;` | Executes statement at block exit |
| Class Declaration | `vaguppu` | `class` | OOP class with inheritance |
| Struct Declaration | `kattamaipu` | `struct` | C-compatible data structure |
| Enum Declaration | `enumaripu` | `enum` | Named integer enumerations |
| Union Declaration | `ondrippu` | `union` | Overlapping memory structure |
| Type Definition | `typedef` | `typedef` | Type alias |
| Current Instance | `suya` | `this` | Pointer to current instance |
| Superclass Access | `thai`, `moolam` | `super` | Parent class method dispatcher |
| Heap Allocation | `pudhiya` | `new` | Dynamic object creation |
| Heap Deallocation | `azhi` | `delete` | Explicit memory deallocation |
| Null Pointer | `maru` | `null` | Null pointer constant |
| Size Of Type | `alavu(T)` | `sizeof(T)` | Size in bytes of type |
| Type Casting | `mathu<T>(x)` | `cast<T>(x)`, `(T)x` | Explicit type conversion |
| Constant Storage | `maradha` | `const` | Immutable qualifier |
| Static Storage | `nilaiyana` | `static` | Static storage duration |
| Module Include | `serku "..."` | `import "..."` | Include header or binding |
| External Symbol | `veli` | `extern` | C library FFI declaration |

---

## 3. Comments & Program Structure

- **Line comments**: `# comment text` or `// comment text`
- **Block comments**: `/* multiline comment */`
- **Entry point**: Every executable program requires a `seyal main(): int` or `seyal main(): void` function.

```tamizhi
# Single line comment
// C-style single line comment
/*
   Multi-line block comment
*/

seyal main(): int {
    pathi("Vanakkam, World!\n");
    kudu 0;
}
```

---

## 4. Variables, Constants & Scope

### Variable Declarations (`edu` / `var` / `let`)
```tamizhi
# Type inferred from initialization:
edu count = 42;
edu name = "Tamizhi";

# Explicit type annotation:
edu score: int = 100;
edu pi: float = 3.1415926535;
edu letter: char = 'A';
edu active: bool = true;
```

### Immutable Constants (`const` / `maradha`)
```tamizhi
const edu MAX_LIMIT: int = 1000;
# or
maradha edu APP_NAME: string = "Tamizhi Engine";
```

### Static Storage (`static` / `nilaiyana`)
Preserves variable value across function calls:
```tamizhi
seyal get_next_id(): int {
    static edu next_id: int = 1;
    edu current = next_id;
    next_id += 1;
    kudu current;
}
```

---

## 5. Type System

### Primitive Types
- `int`: 64-bit signed integer.
- `float`, `double`: 64-bit IEEE-754 double-precision floating point.
- `char`: 8-bit character.
- `bool`: Boolean (`true` or `false`).
- `void`: Empty / unit type.
- `string`: Immutable pointer-backed string (`const char*`).

### Unsigned & Fixed-Width Types
```tamizhi
edu u1: uint = 4000000000;
edu b1: uint8 = 255;
edu s1: int16 = -32000;
edu i3: int32 = 1000000;
edu i6: int64 = 9223372036854775807;
edu sz: size_t = 1024;
```

### Pointers & References
```tamizhi
edu value: int = 50;
edu ptr: int* = &value;    # Address-of (&)
*ptr = 100;                # Dereference (*)
pathi("New value: %d\n", value); # prints 100
```

### Arrays & Slices
```tamizhi
# Fixed-size array with initializer:
edu numbers: int[5] = [10, 20, 30, 40, 50];

# Array indexing (0-based):
numbers[0] = 99;

# Query array length:
edu len = numbers.length();  # or numbers.alavu()
```

### Type Aliases (`typedef`)
```tamizhi
typedef int Metric;
typedef Vector2* VectorPtr;

edu distance: Metric = 150;
```

---

## 6. Functions & Methods

### Function Definition
```tamizhi
# Syntax: seyal <name>(<param1>: <type1>, ...): <return_type> { ... }
seyal add(a: int, b: int): int {
    kudu a + b;
}
```

### Default Parameter Values
Parameters with default values can be omitted by callers:
```tamizhi
seyal greet_user(name: string, prefix: string = "User", exclaim: bool = true): void {
    if (exclaim) {
        pathi("Welcome, %s %s!\n", prefix, name);
    } else {
        pathi("Welcome, %s %s.\n", prefix, name);
    }
}

# Usages:
greet_user("Sibi");                         # "Welcome, User Sibi!"
greet_user("Sibi", "Admin");                # "Welcome, Admin Sibi!"
greet_user("Sibi", "Guest", false);         # "Welcome, Guest Sibi."
```

### Forward Declarations / Prototypes
```tamizhi
# Prototype declaration (no body):
seyal compute_area(width: int, height: int): int;

# Definition:
seyal compute_area(width: int, height: int): int {
    kudu width * height;
}
```

---

## 7. Operators & Expressions

### Arithmetic, Logical & Bitwise Operators
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Comparison**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **Logical**: `&&`, `||`, `!`
- **Bitwise**: `&` (AND), `|` (OR), `^` (XOR), `~` (NOT), `<<` (shl), `>>` (shr)
- **Compound Assignment**: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- **Increment/Decrement**: `++x`, `x++`, `--x`, `x--`

### Ternary Operator (`? :`)
```tamizhi
edu max_val = (a > b) ? a : b;
```

### Null-Coalescing Operator (`??`)
Evaluates to the left-hand operand if not null; otherwise returns the right-hand fallback:
```tamizhi
edu input_name: string = null;
edu display_name = input_name ?? "Default User";

edu raw_ptr: int* = null;
edu fallback_val: int = 0;
edu safe_ptr = raw_ptr ?? &fallback_val;
```

### Type Casting (`(T)`, `cast<T>`, `mathu<T>`)
```tamizhi
edu f: float = 42.9;
edu i1: int = (int)f;
edu i2: int = cast<int>(f);
edu i3: int = mathu<int>(f);
```

### Memory Size (`sizeof` / `alavu`)
```tamizhi
edu sz1 = sizeof(int);      # 8 (64-bit int)
edu sz2 = alavu(double);
```

---

## 8. String Handling & Interpolation

### String Interpolation (`f"..."`)
Embed variables and arbitrary expressions directly inside string literals using `{...}`:
```tamizhi
edu user: string = "Tamizhi";
edu major = 2;
edu minor = 4;

# Interpolate variables:
edu header = f"Running {user} v{major}.{minor}!";

# Interpolate expressions and calculations:
edu math_text = f"Result of 10 + 20 is {10 + 20}, sqrt(100) = {math_sqrt(100.0)}";

# Escaped braces (\{, \}, {{, }}):
edu brace_sample = f"Literal \{ brace \} and {{double}}";
```

### String Concatenation & Compound Assignment (`+`, `+=`)
```tamizhi
edu s = "Hello, " + "World!";
s += " Welcome to Tamizhi!";
```

### String Character Indexing (`s[i]`)
Access characters in strings via standard 0-based indexing (strings are immutable):
```tamizhi
edu s = "Tamizhi";
edu first: char = s[0];   # 'T'
```

### String Intrinsics & Method Calls
Strings support both intrinsic functions and object method calls:
```tamizhi
edu s: string = "hello world";

# Length:
edu l1 = s.length();                     # 11
edu l2 = s.alavu();                      # 11
edu l3 = str_len(s);                     # 11

# Substring (start, length):
edu sub1 = s.sub(6, 5);                   # "world"
edu sub2 = str_sub(s, 0, 5);              # "hello"

# Substring Search & Containment:
edu idx = s.find("world");               # 6 (returns -1 if not found)
edu has = s.contains("world");           # or s.adangiullathu(...) -> true
edu start = s.starts_with("hel");        # or s.thodangugiradhu(...) -> true
edu end = s.ends_with("rld");            # or s.mudigiradhu(...) -> true

# Case Conversion:
edu upper = s.to_uppercase();            # or s.periya_ezhuthu() -> "HELLO WORLD"
edu lower = s.to_lowercase();            # or s.siriya_ezhuthu() -> "hello world"

# Trimming & Replacement:
edu clean = "  spaced  ".trim();         # or .vetru() -> "spaced"
edu rep = s.replace("world", "tamizhi"); # or s.matru(...) -> "hello tamizhi"
```

---

## 9. Control Flow

### Conditionals (`if`, `else if`, `else`)
```tamizhi
# English style:
if (score >= 90) {
    pathi("Outstanding\n");
} else if (score >= 60) {
    pathi("Passed\n");
} else {
    pathi("Try Again\n");
}

# Tanglish style:
@(score >= 90) aga irundha {
    pathi("Sirappu\n");
} ^(score >= 60) aga irundha {
    pathi("Vetri\n");
} illana {
    pathi("Meendum muyal\n");
}
```

### Standard Loops (`while`, `for`, `do-while`)
```tamizhi
# While loop:
edu i = 0;
while (i < 5) {
    pathi("%d\n", i);
    i++;
}

# Standard 3-part for loop:
for (edu j = 0; j < 5; j++) {
    pathi("%d\n", j);
}

# Do-While loop:
edu k = 0;
do {
    pathi("%d\n", k);
    k++;
} while (k < 5);
```

### Integer Ranges & Range-Based Loops
Iterate directly over integer ranges without manual index incrementing:
```tamizhi
# Half-open range 0..5 generates: 0, 1, 2, 3, 4
for (edu i in 0..5) {
    pathi("%d ", i);
}

# Closed/inclusive range 1..=5 generates: 1, 2, 3, 4, 5
padigal (edu j ul 1..=5) {
    pathi("%d ", j);
}

# Iterating arrays:
edu list: int[3] = [10, 20, 30];
for (edu val in list) {
    pathi("Value: %d\n", val);
}

# Iterating string characters:
for (edu c in "tamizhi") {
    pathi("%c ", c);
}
```

### Switch Statements (`switch`, `case`, `default`)
```tamizhi
switch (status_code) {
    case 200: {
        pathi("OK\n");
        break;
    }
    case 404: {
        pathi("Not Found\n");
        break;
    }
    default: {
        pathi("Unknown Status\n");
        break;
    }
}
```

### Scope-Exit Cleanup (`defer` / `pinbu`)
Ensures that resource cleanup statements run reliably upon block or function return:
```tamizhi
seyal process_data(file_path: string): int {
    pathi("Opening file\n");
    defer pathi("File closed and resources cleaned\n");

    if (!file_exists(file_path)) {
        pathi("Error: missing file\n");
        kudu -1;   # The deferred statement runs BEFORE returning!
    }

    # Normal execution...
    kudu 0;
}
```

---

## 10. Object-Oriented Programming (OOP)

### Class Declarations, Constructors & Methods
```tamizhi
vaguppu Point {
    x: int;
    y: int;

    # Constructor: named 'init', 'uruvakku', or 'constructor'
    seyal init(x: int, y: int) {
        this.x = x;
        this.y = y;
    }

    seyal print_coords(): void {
        pathi("Point(%d, %d)\n", this.x, this.y);
    }
}
```

### Inheritance & Super Calls (`super` / `thai`)
```tamizhi
vaguppu Animal {
    name: string;

    seyal init(name: string) {
        this.name = name;
    }

    seyal speak(): void {
        pathi("%s makes a sound\n", this.name);
    }
}

vaguppu Dog : Animal {
    breed: string;

    seyal init(name: string, breed: string) {
        this.name = name;
        this.breed = breed;
    }

    seyal speak(): void {
        super.speak(); # Calls Animal::speak()
        pathi("%s barks\n", this.name);
    }
}
```

### Heap Allocation (`new` / `delete`)
```tamizhi
seyal main(): int {
    # Dynamically allocate on heap:
    edu dog: Dog* = new Dog("Raja", "Hound");
    dog->speak();

    # Free memory (compatible with both --gc and --no-gc):
    delete dog; # or azhi dog;
    kudu 0;
}
```

---

## 11. Composite Data Types

### Structs (`struct` / `kattamaipu`)
Value types that support field storage and inline methods:
```tamizhi
kattamaipu Vector2 {
    x: float;
    y: float;

    seyal length_sq(): float {
        kudu this.x * this.x + this.y * this.y;
    }
}

edu v: Vector2;
v.x = 3.0;
v.y = 4.0;
pathi("Length squared: %f\n", v.length_sq());
```

### Enums (`enum` / `enumaripu`)
```tamizhi
enumaripu Color {
    RED = 0,
    GREEN = 1,
    BLUE = 2
}

edu c: Color = Color.GREEN;
```

### Unions (`union` / `ondrippu`)
```tamizhi
ondrippu DataSlot {
    as_int: int;
    as_float: float;
}
```

---

## 12. Built-in Intrinsics

### Console Output (`pathi`, `print`, `println`)
Supports standard format specifiers (`%d`, `%f`, `%s`, `%c`, `%x`, etc.):
```tamizhi
pathi("Integer: %d, Float: %f, String: %s\n", 42, 3.14, "hello");
```

### Math Intrinsics
Native, fast inline mathematical routines:
| Function | Signature | Description |
|---|---|---|
| `math_sqrt(x)` | `(float): float` | Square root |
| `math_pow(x, y)` | `(float, float): float` | Base $x$ raised to power $y$ |
| `math_floor(x)` | `(float): float` | Floor (largest integer $\le x$) |
| `math_ceil(x)` | `(float): float` | Ceiling (smallest integer $\ge x$) |
| `math_round(x)` | `(float): float` | Rounds $x$ to nearest integer |
| `math_abs(x)` | `(int): int` | Absolute integer value |
| `math_min(a, b)` | `(int, int): int` | Minimum of two integers |
| `math_max(a, b)` | `(int, int): int` | Maximum of two integers |

```tamizhi
edu s = math_sqrt(25.0);       # 5.0
edu p = math_pow(2.0, 4.0);    # 16.0
edu fl = math_floor(3.9);      # 3.0
edu ce = math_ceil(3.1);       # 4.0
edu ro = math_round(3.5);      # 4.0
edu a = math_abs(-42);         # 42
edu m = math_max(10, 20);      # 20
```

### Type Conversion & Parsing Intrinsics
| Function | Tanglish Alias | Signature | Description |
|---|---|---|---|
| `str_to_int(s)` | `en(s)` | `(string): int` | Parses string as signed 64-bit integer |
| `str_to_float(s)` | `pulli_en(s)` | `(string): float` | Parses string as 64-bit float |
| `int_to_str(n)` | `ezhuthu(n)` | `(int): string` | Converts integer to string |
| `float_to_str(f)` | — | `(float): string` | Converts float to string |
| `char_to_str(c)` | — | `(char): string` | Converts character to 1-character string |

```tamizhi
edu i: int = str_to_int("1234");       # or en("1234")
edu f: float = str_to_float("3.1415"); # or pulli_en("3.1415")
edu s_int: string = int_to_str(42);    # or ezhuthu(42)
```

### System & Environment Intrinsics
| Function | Tanglish Alias | Signature | Description |
|---|---|---|---|
| `sys_time()` | `neram()` | `(): int` | Returns current UNIX timestamp in seconds |
| `sys_getenv(name)` | `soozhal(name)` | `(string): string` | Retrieves environment variable value |
| `sys_system(cmd)` | `kattalai(cmd)` | `(string): int` | Executes host shell command and returns exit code |

```tamizhi
edu timestamp = sys_time();                   # or neram()
edu user = sys_getenv("USER");                # or soozhal("USER")
edu exit_status = sys_system("echo 'Done'");  # or kattalai("echo 'Done'")
```

### File I/O Intrinsics
Native disk operations with zero external library overhead:
| Function | Signature | Description |
|---|---|---|
| `file_write(path, data)` | `(string, string): bool` | Writes text to a file (creates or overwrites) |
| `file_exists(path)` | `(string): bool` | Checks if a file exists on disk |
| `file_read(path)` | `(string): string` | Reads entire file into a string |

```tamizhi
edu file_name: string = "config.txt";
edu saved: bool = file_write(file_name, "version=2.4.0\nactive=true\n");

if (file_exists(file_name)) {
    edu content: string = file_read(file_name);
    pathi("Config Content:\n%s", content);
}
```

---

## 13. Modules, Headers & C FFI

### Tamizhi Headers (`.tzh`)
Organize projects across multiple source and header files using `serku` / `import`:

`geometry.tzh`:
```tamizhi
typedef int Coord;

kattamaipu Rect {
    w: Coord;
    h: Coord;
}

seyal rect_area(r: Rect*): int;
```

`main.tz`:
```tamizhi
serku "geometry.tzh";

seyal rect_area(r: Rect*): int {
    kudu r->w * r->h;
}

seyal main(): int {
    edu r: Rect;
    r.w = 10;
    r.h = 20;
    pathi("Area = %d\n", rect_area(&r));
    kudu 0;
}
```

### C Foreign Function Interface (`extern` / `veli`)
Directly call external C runtime or system functions:
```tamizhi
extern header "<time.h>";
extern seyal time(t: void*): int;

seyal main(): int {
    edu t = time(null);
    pathi("Unix timestamp: %d\n", t);
    kudu 0;
}
```

---

## 14. Compiler Options & CLI Reference

```sh
# Standard compilation:
./tamizhi input.tz -o output_bin

# Inspect generated C99 code:
./tamizhi input.tz -S

# Compile without Boehm Garbage Collector (uses standard malloc/free):
./tamizhi input.tz --no-gc -o output_bin

# Optimization levels:
./tamizhi input.tz -O3 -o output_bin

# Include search paths:
./tamizhi input.tz -I./include -L./lib -lmylib -o output_bin

# Check compiler version:
./tamizhi --version
```

