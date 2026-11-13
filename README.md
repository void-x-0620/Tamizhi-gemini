# Tamizhi (v2.5.0)

A compiler for Tamizhi (a Tanglish and C-compatible language) written in C.
It translates Tamizhi source directly to optimized C99 and compiles with the host C compiler (GCC/Clang), producing high-performance native binaries with zero interpretation overhead.

Version 2.5.0 introduces the **Comprehensive Tamizhi Standard Library (`std`)**, **Standard Include & Library Resolution Path Hierarchy**, and **Installation Targets (`make install`, `make install-user`)**:
- **`std/math.tzh`**: High-precision trigonometric functions (`sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`), constants (`MATH_PI`, `MATH_E`, `MATH_TAU`), rounding (`floor`, `ceil`, `round`), clamping (`clamp`, `clamp_int`), `lerp`, `hypot`, and Tamil aliases.
- **`std/string.tzh`**: Character classification (`is_digit`, `is_alpha`, `is_space`), string repeat, left/right padding, string reversal, and mutable `StringBuilder` class.
- **`std/io.tzh`**: Cross-platform path helpers (`path_combine`, `path_basename`, `path_dirname`, `path_extension`), file utilities (`file_append`, `file_remove`, `file_size`), and directory management (`dir_create`, `dir_remove`).
- **`std/os.tzh`**: Process management (`os_pid`, `os_cwd`, `os_exit`, `os_setenv`, `os_getenv`, `os_exec`, `os_name`, `os_temp_dir`).
- **`std/time.tzh`**: High-precision sleep (`time_sleep_ms`, `time_sleep_sec`), timestamps (`time_now`, `time_clock`), and object-oriented `Stopwatch` timer.
- **`std/random.tzh`**: PRNG utilities (`rand_seed`, `rand_int`, `rand_range`, `rand_float`, `rand_bool`, `rand_choice_int`).
- **`std/collections.tzh`**: Dynamic data structures (`IntList`, `StringList` with `push`, `pop`, `get`, `set`, `length`, `contains`, `sum`, `join`).
- **`std/testing.tzh`**: Zero-dependency assertion framework (`assert_true`, `assert_false`, `assert_eq_int`, `assert_eq_str`, `assert_eq_float`, `test_summary`).
- **Umbrella headers**: `serku "std.tzh";` or `serku "tamizhi.tzh";` to import the entire standard library with one line.
- **Search Path Resolution**: Automatic resolution across local project `./include`, user-local `~/.local/share/include/tamizhi`, and system `/usr/include/tamizhi`, plus library lookup in `~/.local/share/lib` and `/usr/lib/tamizhi`.

---

## Build and Run

```sh
make                      # build ./tamizhi
make test                 # run all 50 test suites (program tests, error tests, ASan)
make asan                 # build ./tamizhi-asan (AddressSanitizer + UBSan)
./tamizhi prog.tz -o prog && ./prog
```

Compiler options:
- `-o <file>` : Output binary (default: `a.out`)
- `-S` : Emit generated C99 code
- `-c` : Compile to object file (`.o`)
- `-O<0-3>` : Optimization level for host C compiler (default: `-O2`)
- `-v` : Verbose compilation timings
- `-I<dir>`, `-L<dir>`, `-l<name>` : Include directories, library directories, and libraries
- `--gc` / `--no-gc` : Enable/disable Boehm GC (default: `--gc`)
- `--version` : Print compiler version

---

## Complete Syntax Reference

### 1. Keywords & Aliases

Tamizhi supports dual Tanglish and English / C keywords:

| Construct | Tanglish Keyword | C / English Alias |
|---|---|---|
| Variable declaration | `edu` | `var`, `let` |
| Function declaration | `seyal` | `fn`, `func`, `function` |
| Return statement | `kudu` | `return` |
| Print output | `pathi(...)` | `print(...)`, `println(...)` |
| Class declaration | `vaguppu` | `class` |
| Struct declaration | `kattamaipu` | `struct` |
| Enum declaration | `enumaripu` | `enum` |
| Union declaration | `ondrippu` | `union` |
| Type definition | `typedef` | `typedef` |
| Instance reference | `suya` | `this` |
| Base class reference | `thai`, `moolam` | `super` |
| Heap instantiation | `pudhiya` | `new` |
| Heap deallocation | `azhi` | `delete` |
| Null pointer | `maru` | `null` |
| Sizeof operator | `alavu` | `sizeof` |
| Type cast | `mathu<T>` | `cast<T>` |
| Const qualifier | `maradha` | `const` |
| Static storage | `nilaiyana` | `static` |
| If conditional | `@(cond) aga irundha` | `if (cond)` |
| Else-if conditional | `^(cond) aga irundha` | `else if (cond)` |
| Else branch | `illana` | `else` |
| While loop | `sutri (cond)` | `while (cond)` |
| For loop | `padigal (...)` | `for (...)` |
| Do-while loop | `sei { ... } sutri (...)` | `do { ... } while (...)` |
| Switch statement | `therivu (expr)` | `switch (expr)` |
| Case branch | `nilai val:` | `case val:` |
| Default branch | `muthal:` | `default:` |
| Loop break | `udai;` | `break;` |
| Loop continue | `thodar;` | `continue;` |
| Scope-exit defer | `pinbu <stmt>;` | `defer <stmt>;` |
| Range-based iteration | `ul` | `in` |
| Binding include | `serku` | `import` |
| C FFI extern | `veli` | `extern` |

---

### 2. Supported Types

- **Primitives**: `int` (64-bit), `float` (64-bit double), `double`, `char`, `short`, `long`, `void`, `bool`, `string`
- **C-Equivalent Types (`c` prefix)**: `cint` (32-bit int), `cfloat` (32-bit float), `cdouble` / `cdouple` (64-bit double), `clong`, `clonglong`, `cshort`, `cchar`, `cuchar`, `cushort`, `cuint`, `culong`, `cbool`, `cvoid`, `csize_t` / `csize`
- **Unsigned types**: `unsigned char` (`uchar`), `unsigned short` (`ushort`), `unsigned int` (`uint`), `unsigned long` (`ulong`)
- **Exact-width types**: `int8` (`int8_t`), `int16` (`int16_t`), `int32` (`int32_t`), `int64` (`int64_t`), `uint8` (`uint8_t`), `uint16` (`uint16_t`), `uint32` (`uint32_t`), `uint64` (`uint64_t`)
- **System types**: `size_t`, `ssize_t`, `intptr_t`, `uintptr_t`, `ptrdiff_t`
- **Pointers**: `T*`, `T**` (e.g. `int*`, `cint*`, `cfloat*`, `char*`, `void*`, `Vector2*`)
- **Arrays**: Fixed-size `T[N]` and unsized `T[]`
- **User types**: Structs (`kattamaipu` / `struct`), Classes (`vaguppu` / `class`), Enums (`enumaripu` / `enum`), Unions (`ondrippu` / `union`), and `typedef` aliases.

---

### 3. Object-Oriented Programming (OOP)

#### Classes and Single Inheritance
```tamizhi
vaguppu Animal {
    name : string;
    age  : int;

    # Constructor: named 'uruvakku', 'init', or 'constructor'
    seyal uruvakku(name : string, age : int) {
        this.name = name;
        this.age = age;
    }

    seyal speak() : void {
        pathi("%s makes a sound, age %d\n", this.name, this.age);
    }
}

# Dog inherits fields and methods from Animal
vaguppu Dog : Animal {
    breed : string;

    seyal uruvakku(name : string, age : int, breed : string) {
        this.name = name;
        this.age = age;
        this.breed = breed;
    }

    seyal speak() : void {
        pathi("%s the %s barks!\n", this.name, this.breed);
    }

    seyal fetch() : void {
        pathi("%s fetches the ball!\n", this.name);
    }
}
```

#### Instantiation & Method Calls
```tamizhi
# Stack allocation with automatic constructor call
edu a : Animal = Animal("Leo", 4);
a.speak();

# Heap allocation using 'new' / 'pudhiya'
edu d : Dog* = new Dog("Buddy", 3, "Golden Retriever");
d->speak();
d->fetch();
```

---

### 4. Structs with Methods

Structs (`kattamaipu` / `struct`) support member methods with `this` / `suya`:

```tamizhi
kattamaipu Vector2 {
    x : int;
    y : int;

    seyal length_sq() : int {
        kudu this.x * this.x + this.y * this.y;
    }

    seyal scale(factor : int) : void {
        this.x = this.x * factor;
        this.y = this.y * factor;
    }
}

seyal main() : int {
    edu v : Vector2;
    v.x = 3;
    v.y = 4;
    pathi("length_sq = %d\n", v.length_sq());

    edu vp : Vector2* = &v;
    vp->scale(2);
    pathi("scaled = (%d, %d)\n", vp->x, vp->y);
    kudu 0;
}
```

---

### 5. Enums and Unions

#### Enums (`enumaripu` / `enum`)
```tamizhi
enum Status {
    PENDING = 0,
    RUNNING = 1,
    COMPLETED = 10,
    FAILED
}

edu s : Status = RUNNING;
```

#### Unions (`ondrippu` / `union`)
```tamizhi
union Data {
    i : int;
    f : float;
    c : char;
}

edu d : Data;
d.i = 42;
```

#### Typedef
```tamizhi
typedef int Integer;
typedef Vector2* VectorPtr;
```

---

### 6. Storage Qualifiers: `const` and `static`

```tamizhi
const edu MAX_RETRIES : int = 3;
static edu global_counter : int = 0;

static seyal helper_func() : void {
    static edu call_count : int = 0;
    const edu step : int = 1;
    call_count += step;
}
```

---

### 7. Pointers & Operators

- **Address-of**: `&var`
- **Dereference**: `*ptr`
- **Arrow member access**: `ptr->member`
- **Bitwise**: `&` (AND), `|` (OR), `^` (XOR), `~` (NOT), `<<` (shift left), `>>` (shift right)
- **Compound Assignment**: `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`
- **Increment/Decrement**: `++x`, `x++`, `--x`, `x--`
- **Ternary Operator**: `cond ? then_val : else_val`

```tamizhi
seyal swap(a : int*, b : int*) : void {
    edu tmp : int = *a;
    *a = *b;
    *b = tmp;
}
```

---

### 8. Control Flow

#### For Loop (`padigal` / `for`)
```tamizhi
for (edu i : int = 0; i < 10; i++) {
    pathi("i = %d\n", i);
}
```

#### Do-While Loop (`sei` / `do`)
```tamizhi
do {
    count++;
} while (count < 5);
```

#### Switch / Case / Default (`therivu` / `nilai` / `muthal`)
```tamizhi
switch (choice) {
    case 1: {
        pathi("Option 1\n");
        break;
    }
    case 2: {
        pathi("Option 2\n");
        break;
    }
    default: {
        pathi("Default option\n");
        break;
    }
}
```

#### Conditionals
Both Tamizhi style and C style are supported:
```tamizhi
# Tamizhi style
@(score >= 90) aga irundha {
    pathi("A\n");
} ^(score >= 80) aga irundha {
    pathi("B\n");
} illana {
    pathi("C\n");
}

# C style
if (score >= 90) {
    pathi("A\n");
} else if (score >= 80) {
    pathi("B\n");
} else {
    pathi("C\n");
}
```

---

### 9. Tamizhi Header Files (`.tzh`) & Modular Code

Tamizhi supports user-defined header files (`.tzh`) and multi-file project organization via `serku` / `import`.

#### Defining a Header File (`math_utils.tzh`)
```tamizhi
# Typedefs, structs, classes, and enums can be shared
typedef int Number;

kattamaipu Point {
    x : int;
    y : int;
    seyal dist_sq() : int {
        kudu this.x * this.x + this.y * this.y;
    }
}

# Function prototypes (forward declarations without bodies)
seyal compute_add(a : int, b : int) : int;

# Full inline functions in headers
seyal compute_mul(a : int, b : int) : int {
    kudu a * b;
}
```

#### Including in Your Main Program (`main.tz`)
```tamizhi
serku "math_utils.tzh";

# Implementation of the forward-declared prototype
seyal compute_add(a : int, b : int) : int {
    kudu a + b;
}

seyal main() : int {
    edu p : Point;
    p.x = 3;
    p.y = 4;
    pathi("add: %d, mul: %d, dist: %d\n", compute_add(10, 5), compute_mul(10, 5), p.dist_sq());
    kudu 0;
}
```

---

### 10. Advanced Language Features (v2.1.0)

#### Character & Number Literals
```tamizhi
edu c : char = 'Z';
edu hex : int = 0xFF;        # 255
edu bin : int = 0b1011;      # 11
edu oct : int = 0o77;        # 63
edu sci : float = 1.5e2;     # 150.0
```

#### Null Pointer Literals (`null` / `maru`)
```tamizhi
edu ptr : int* = null;
edu ptr2 : int* = maru;
if (ptr == null && ptr2 == maru) {
    pathi("Pointers are null\n");
}
```

#### `sizeof` / `alavu` and Type Casting
```tamizhi
edu sz : int = sizeof(int);           # or alavu(int)
edu val : int = cast<int>(3.14);      # or mathu<int>(3.14) or (int)3.14
```

#### OOP `super` / `thai` and Deallocation (`delete` / `azhi`)
```tamizhi
vaguppu Animal {
    name : string;
    seyal speak() : void { pathi("Animal speaks\n"); }
}

vaguppu Dog : Animal {
    seyal speak() : void {
        super.speak();                # invoke parent class method
        pathi("Dog barks\n");
    }
}

seyal main() : int {
    edu d : Dog* = new Dog();
    d->speak();
    delete d;                         # or azhi d; works with both --gc and --no-gc
    kudu 0;
}
```

---

### 10. Advanced Language Features (v2.2.0)

#### Scope-Exit Cleanup (`defer` / `pinbu`)
Executes statements in LIFO (last-in, first-out) order when the enclosing block or function returns/exits:
```tamizhi
seyal process_file(path: string): int {
    edu f = open_file(path);
    defer close_file(f);              # guaranteed to execute on exit or early return
    defer pathi("cleaning up f\n");

    if (error_occurred()) {
        kudu -1;                      # defers unwind automatically before return
    }
    kudu 0;
}
```

#### Range-Based Loops (`for ... in ...` / `padigal ... ul ...`)
Iterate directly over fixed-size arrays, slices, pointers, or string characters:
```tamizhi
edu nums: int[4] = [10, 20, 30, 40];
for (edu n in nums) {
    pathi("%d\n", n);
}

# Tanglish keyword form:
padigal (edu x ul nums) {
    if (x == 30) thodar;              # continue
    pathi("item: %d\n", x);
}

# String character iteration:
for (edu c in "tamizhi") {
    pathi("%c ", c);
}
```

#### Default Parameter Values
Function and method parameters can declare default expressions:
```tamizhi
seyal compute(a: int, b: int = 10, c: int = 100): int {
    kudu a + b + c;
}

compute(1);            # evaluates to 1 + 10 + 100 = 111
compute(1, 2);         # evaluates to 1 + 2 + 100 = 103
compute(1, 2, 3);      # evaluates to 1 + 2 + 3 = 6
```

#### Built-in String Intrinsics & Methods
Native string utilities available directly or as methods on `string`:
```tamizhi
edu s: string = "hello world";
edu len1 = str_len(s);         # 11
edu len2 = s.length();         # 11
edu len3 = s.alavu();          # 11

edu sub = s.sub(6, 5);         # "world"
edu idx = s.find("world");     # 6 (or -1 if not found)
```

#### Enhanced Diagnostic Reporting
The compiler produces Clang and Rustc-style diagnostics with line number gutters, source snippets, caret pointers, terminal ANSI coloring, and fuzzy "did you mean?" identifier and field suggestions:
```
example.tz:4:11: error: undeclared identifier 'y'; did you mean 'x'?
    4 |     edu z = y + 1;
      |             ^
```

---

### 11. Modern Advanced Language Features (v2.3.0)

#### Integer Range Loops (`..` and `..=`)
Iterate with half-open (`start..end`) or closed/inclusive (`start..=end`) ranges without manual loop index management:
```tamizhi
# Half-open range (0, 1, 2, 3, 4):
for (edu i in 0..5) {
    pathi("%d ", i);
}

# Inclusive range (1, 2, 3, 4, 5) with Tanglish keywords:
padigal (edu j ul 1..=5) {
    pathi("%d ", j);
}
```

#### Null-Coalescing Operator (`??`)
Evaluates to the right-hand operand when the left-hand operand is null:
```tamizhi
edu name: string = null;
edu display: string = name ?? "Anonymous";

edu ptr: int* = null;
edu fallback_val: int = 100;
edu safe_ptr: int* = ptr ?? &fallback_val;
```

#### String Interpolation (`f"..."`)
Embed variables and arbitrary expressions directly into strings with `{expr}`:
```tamizhi
edu lang: string = "Tamizhi";
edu major: int = 2;
edu minor: int = 3;

edu greeting: string = f"Language: {lang} v{major}.{minor}!";
# "Language: Tamizhi v2.3!"

edu summary: string = f"2 + 3 = {2 + 3}, sqrt(16) = {math_sqrt(16.0)}";
```

#### Array `.length()` and `.alavu()` Methods
Query the length of fixed-size arrays cleanly as a method:
```tamizhi
edu numbers: int[5] = [10, 20, 30, 40, 50];
pathi("length: %d\n", numbers.length());   # 5
pathi("alavu: %d\n", numbers.alavu());     # 5
```

#### Native Math Intrinsics
High-performance inline math utilities backed by the C runtime library:
```tamizhi
edu s: float = math_sqrt(25.0);       # 5.0
edu p: float = math_pow(2.0, 3.0);    # 8.0
edu a: int = math_abs(-42);           # 42
edu lo: int = math_min(10, 20);       # 10
edu hi: int = math_max(10, 20);       # 20
```

#### Native File I/O Intrinsics
Read and write text or binary files directly from Tamizhi:
```tamizhi
edu path: string = "data.txt";
edu ok: bool = file_write(path, "Hello from Tamizhi!");
if (file_exists(path)) {
    edu content: string = file_read(path);
    pathi("Read %s: %s\n", path, content);
}
```

---

### 12. Advanced Language Features & Enhancements (v2.4.0)

#### String Compound Concatenation (`+=`)
Append directly to strings with automatic heap management:
```tamizhi
edu s: string = "Hello";
s += " World";
pathi("%s\n", s);   # "Hello World"
```

#### String Character Indexing (`s[i]`)
Access any character by index (strings remain safely immutable):
```tamizhi
edu word: string = "Tamizhi";
edu first_char: char = word[0];   # 'T'
```

#### Extended String Methods & Tanglish Aliases
```tamizhi
edu s: string = "  Hello Tamizhi World  ";

# Search & Match:
edu has_tamizhi: bool = s.contains("Tamizhi");        # or s.adangiullathu("Tamizhi")
edu starts_h: bool = s.starts_with("  Hel");          # or s.thodangugiradhu(...)
edu ends_w: bool = s.ends_with("World  ");            # or s.mudigiradhu(...)

# Transformations:
edu clean: string = s.trim();                         # or s.vetru() -> "Hello Tamizhi World"
edu loud: string = clean.to_uppercase();              # or clean.periya_ezhuthu()
edu quiet: string = clean.to_lowercase();             # or clean.siriya_ezhuthu()
edu replaced: string = clean.replace("World", "Lang"); # or clean.matru(...)
```

#### Type Conversion & Parsing Intrinsics
```tamizhi
edu n1: int = str_to_int("42");           # or en("42")
edu f1: float = str_to_float("3.14");     # or pulli_en("3.14")
edu s_int: string = int_to_str(100);      # or ezhuthu(100)
edu s_flt: string = float_to_str(2.718);
edu s_chr: string = char_to_str('Z');
```

#### Extended Math Intrinsics
```tamizhi
edu f: float = math_floor(3.9);    # 3.0
edu c: float = math_ceil(3.1);     # 4.0
edu r: float = math_round(3.5);    # 4.0
```

#### System & Environment Intrinsics
```tamizhi
edu now: int = sys_time();                  # or neram() -> Unix timestamp
edu home_dir: string = sys_getenv("HOME");  # or soozhal("HOME")
edu code: int = sys_system("echo 'Run!'");  # or kattalai("...")
```

---

### 15. Standard Library Modules (`std/*.tzh`)

Tamizhi includes a modular standard library accessible via `serku` (or `import`):

```tamizhi
# Import individual modules:
serku "std/math.tzh";
serku "std/string.tzh";
serku "std/io.tzh";
serku "std/os.tzh";
serku "std/time.tzh";
serku "std/random.tzh";
serku "std/collections.tzh";
serku "std/testing.tzh";

# Or import all modules at once:
serku "std.tzh";
```

#### Standard Library Modules Reference:

1. **`std/math.tzh`**:
   - Constants: `MATH_PI`, `MATH_E`, `MATH_TAU`, `PI`, `E`
   - Conversions: `deg_to_rad(deg)`, `rad_to_deg(rad)`
   - Utilities: `clamp(v, min, max)`, `clamp_int(v, min, max)`, `lerp(a, b, t)`, `hypot(x, y)`, `min_f`, `max_f`, `abs_f`, `abs_int`, `sign_f`, `sign_int`
   - Trigonometry: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`
   - Exponentials & Roots: `ln`, `exp`, `sqrt`, `pow`, `floor`, `ceil`, `round`
   - Tamil aliases: `vargamulam` (sqrt), `madippu` (abs), `ellai` (clamp)

2. **`std/string.tzh`**:
   - Character classification: `is_digit(c)`, `is_alpha(c)`, `is_alphanumeric(c)`, `is_space(c)`, `to_upper_char(c)`, `to_lower_char(c)`
   - String utilities: `str_repeat(s, n)`, `str_pad_left(s, len, ch)`, `str_pad_right(s, len, ch)`, `str_reverse(s)`
   - `StringBuilder` class:
     ```tamizhi
     edu sb: StringBuilder = StringBuilder();
     sb.append("Tamizhi ");
     sb.append_int(2);
     sb.append_char('!');
     pathi("%s\n", sb.to_str());  # "Tamizhi 2!"
     ```

3. **`std/io.tzh`**:
   - Path helpers: `path_combine(dir, file)`, `path_join(a, b)`, `path_basename(path)`, `path_dirname(path)`, `path_extension(path)`
   - File helpers: `file_append(path, text)`, `file_remove(path)`, `file_size(path)`
   - Directory helpers: `dir_create(path)`, `dir_remove(path)`

4. **`std/os.tzh`**:
   - System metadata: `os_name()`, `os_path_separator()`, `os_temp_dir()`
   - Process control: `os_pid()`, `os_cwd()`, `os_exit(code)`, `os_exec(cmd)`
   - Environment: `os_getenv(key)`, `os_setenv(key, value, overwrite)`

5. **`std/time.tzh`**:
   - High-precision sleep: `time_sleep_ms(ms)`, `time_sleep_sec(sec)`
   - Timestamps: `time_now()`, `time_clock()`
   - `Stopwatch` class:
     ```tamizhi
     edu sw: Stopwatch = Stopwatch();
     sw.start();
     time_sleep_ms(50);
     pathi("Elapsed: %d ms\n", sw.elapsed_ms());
     ```

6. **`std/random.tzh`**:
   - PRNG seed: `rand_seed(seed)`
   - Number generation: `rand_int()`, `rand_range(min, max)`, `rand_float()`, `rand_bool()`, `rand_choice_int(a, b)`

7. **`std/collections.tzh`**:
   - `IntList` class: `push(val)`, `pop()`, `get(idx)`, `set(idx, val)`, `length()`, `is_empty()`, `clear()`, `contains(val)`, `index_of(val)`, `sum()`
   - `StringList` class: `push(s)`, `pop()`, `get(idx)`, `set(idx, s)`, `length()`, `is_empty()`, `clear()`, `contains(s)`, `index_of(s)`, `join(separator)`

8. **`std/testing.tzh`**:
   - Assertions: `assert_true(cond, name)`, `assert_false(cond, name)`, `assert_eq_int(exp, act, name)`, `assert_eq_str(exp, act, name)`, `assert_eq_float(exp, act, eps, name)`
   - Summary reporter: `test_summary()` -> returns count of failed tests.

---

### 16. Search Path Hierarchy & Installation

The Tamizhi compiler automatically resolves headers (`serku`) and libraries (`-L`):

#### Header Search Order:
1. Current file directory (relative includes)
2. Current working directory
3. Explicit `-I<dir>` flags
4. Project `./include/` and `./include/tamizhi/`
5. User-local `~/.local/share/include/tamizhi/` and `~/.local/share/include/`
6. System `/usr/include/tamizhi/` and `/usr/local/include/tamizhi/`
7. Paths specified in `TAMIZHI_INCLUDE_PATH` environment variable

#### Library Search Order:
1. Explicit `-L<dir>` flags
2. User-local `~/.local/share/lib/` and `~/.local/share/tamizhi/lib/`
3. System `/usr/lib/tamizhi/` and `/usr/local/lib/tamizhi/`

#### Installation:
```sh
# System-wide installation (requires sudo / root):
sudo make install           # installs to /usr/bin, /usr/include/tamizhi, /usr/lib/tamizhi

# User-local installation (no root needed):
make install-user      # installs to ~/.local/bin and ~/.local/share/include/tamizhi
```

---

### 17. C FFI, C-Equivalent Types & Custom Type Imports

Tamizhi provides native interoperability with C libraries and header declarations:

#### C-Equivalent Types (`c` prefix)
Tamizhi standard integers (`int`) and floats (`float`) are 64-bit. For direct C FFI compatibility:
- `cint` -> 32-bit signed C `int`
- `cfloat` -> 32-bit single-precision IEEE-754 `float` (supports `1.5f` suffix)
- `cdouble`, `cdouple` -> 64-bit double-precision C `double`
- `clong`, `clonglong` -> C `long`, `long long`
- `cshort`, `cchar`, `cuchar`, `cushort`, `cuint`, `culong`, `cbool`, `cvoid`, `csize_t` / `csize`
- Literal suffixes like `f`, `u`, `l`, `ll` are supported on numbers (e.g. `3.5f`, `100u`, `0xFFULL`).

#### Importing Custom Types (`veli` / `extern`)
Declare external structs, enums, unions, and typedefs directly or inside `.tzb` binding files:
```tamizhi
# Forward / Opaque struct declaration:
veli struct OpaqueHandle;

# Extern struct with C-style fields:
veli struct NativeVec3 {
    cfloat x;
    cfloat y;
    cfloat z;
};

# Extern enum with optional trailing comma:
veli enum Status {
    STATUS_OK = 0,
    STATUS_ERROR = 1,
};
```

#### Dual Field Syntax in Structs and Unions
Both standard C style (`type name;`) and Tamizhi style (`name : type;`) are supported inside `struct` (`kattamaipu`) and `union` (`ondrippu`):
```tamizhi
kattamaipu PointA {
    cint x;
    cint y;
};

kattamaipu PointB {
    x : cint;
    y : cint;
};
```

#### Header Deduplication
When external C headers are included via `veli "header.h";`, Tamizhi's semantic analyzer uses the declared fields for type checking while the code generator suppresses duplicate C struct/enum definitions, eliminating redefinition collisions with `#include`.

---

## Testing

```sh
make test
```
Runs 59 test suites:
- 26 program tests (`tests/01_hello.tz` to `tests/26_c_types_extern_struct.tz`)
- 22 compile-error diagnostic tests (`tests/errors/`)
- C FFI bindings tests (`tests/ext/`)
- Heap and Boehm GC tests (`tests/heap/`)
- C99 Optimizer and constant-folding checks (`tests/opt/`)
- AddressSanitizer and UndefinedBehaviorSanitizer checks (`make asan`)
