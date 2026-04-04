# Pangu Language Reference

## Overview

Pangu is a compiled programming language with an LLVM backend that also supports
direct JIT execution. It is designed for dense, readable program structure:

- top-level files describe module structure,
- mid-level files describe pipeline and type structure,
- low-level files hold concrete algorithms.

**Self-hosting status:** Pangu can compile itself. A bootstrap compiler in
`bootstrap/` reads PGL source, emits C code, and achieves a fixed-point
(3-generation self-bootstrap verified).

## Toolchain

```bash
make linux            # Build the compiler
make tests            # Run all test suites

./build/pangu run     <file.pgl|dir/> [args...]   # JIT execute
./build/pangu compile <file.pgl|dir/> [-o path]   # Compile to native binary
./build/pangu emit-ir <file.pgl|dir/>              # Print LLVM IR
./build/pangu parse   <file.pgl|dir/>              # Parse and dump AST
./build/pangu --help                                # Show usage
```

- `compile` writes to `build/<name>` by default, or `-o path` to override.
- Directory arguments auto-discover `.pgl` files and merge same-package files.

### Diagnostics

All errors use clang-style format with source location and caret:

```text
path/file.pgl:4:13: error: undefined function 'foo'
    println(foo(1));
            ^~~
```

Panic and signal handlers print PGL source-level backtraces (file:line) in both
JIT and AOT modes.

## Lexical Structure

### Comments

```pgl
// line comment
/* block comment */
```

### Literals

```pgl
42          // integer
0xFF        // hex integer
"hello\n"   // string (supports \n \t \\ \" \0 \xNN \0NNN)
'A'         // char literal → integer (ASCII value)
true false  // bool → integer (1 / 0)
```

### Keywords

`package` `import` `as` `type` `struct` `enum` `interface` `func` `pipeline` `impl`
`if` `else` `for` `in` `while` `do` `return` `switch` `case` `default` `match`
`break` `continue` `goto` `try` `catch`
`public` `static` `const` `final` `var` `class` `worker` `switcher`

### Operators

```
+  -  *  /  %          // arithmetic
== != > < >= <=        // comparison (works on int and string)
&& ||  !               // logical (short-circuit)
++ --                   // prefix increment/decrement
=  :=                   // assignment / define-assignment
.  ::                   // field access / enum variant
>> <>                   // stream push / in-place transform
( ) [ ] { } , ; : ->   // delimiters
```

## Types

### Primitive Types

| Type     | LLVM     | Description                     |
|----------|----------|---------------------------------|
| `int`    | `i32`    | 32-bit signed integer           |
| `char`   | `i32`    | Character (alias for int)       |
| `bool`   | `i32`    | Boolean (true=1, false=0)       |
| `string` | `ptr`    | C string pointer                |
| `ptr`    | `ptr`    | Raw pointer (for pipeline state)|

### Struct

```pgl
type Point struct {
    x int;
    y int;
}

// Construction
p := Point{x: 1, y: 2};

// Field access
println(p.x);

// Field mutation
p.x = 10;

// As function parameter and return value
func translate(p Point, dx int) Point {
    return Point{x: p.x + dx, y: p.y};
}
```

Structs support `@annotation` metadata for reflection:

```pgl
@json("point_data")
type Point struct {
    @required
    x int;
    y int;
}
```

### Enum

```pgl
type Color enum { RED, GREEN, BLUE }

// Enum variant access (ordinal integers: 0, 1, 2)
c := Color::RED;
if (c == Color::BLUE) { println("blue"); }
```

### Pipeline Type

```pgl
type CharToToken pipeline(c int)(kind int, text string);
```

Pipeline declarations define a processing stage with input and output parameters.
See [Pipeline System](#pipeline-system) for full details.

## Functions

```pgl
func add(a int, b int) int {
    return a + b;
}

func greet(name string) {
    println(str_concat("Hello, ", name));
}

func main() {
    result := add(1, 2);
    greet("world");
}
```

### Struct Methods (impl blocks)

```pgl
type Counter struct { value int; }

impl Counter {
    func increment(c Counter) Counter {
        return Counter{value: c.value + 1};
    }
}

// Call as StructName.method(args)
c := Counter{value: 0};
c = Counter.increment(c);
```

## Control Flow

### If / Else

```pgl
if (x > 0) {
    println("positive");
} else if (x == 0) {
    println("zero");
} else {
    println("negative");
}
```

### While Loop

```pgl
i := 0;
while (i < 10) {
    println(i);
    i = i + 1;
    if (i == 5) { break; }
}
```

### For Loop

```pgl
// C-style for loop
for (i := 0; i < 10; ++i) {
    if (i % 2 == 0) { continue; }
    println(i);
}

// For-in range loop (iterate 0..N-1)
for i in 5 {
    println(i);  // prints 0, 1, 2, 3, 4
}

// For-in with variable bound
n := 10;
for i in n {
    println(i);
}

// For-in string iteration
for ch in "hello" {
    println(ch);  // prints ASCII codes: 104, 101, 108, 108, 111
}
```

### Switch / Case

```pgl
switch (x) {
    case 1: println("one");
    case 2: println("two");
    default: println("other");
}
```

### Match Expression

```pgl
type Color enum { RED, GREEN, BLUE }

name := match (c) {
    Color::RED   => "red",
    Color::GREEN => "green",
    _            => "other",
};
```

Match supports enum variants, integer literals, and wildcard `_`.

## Modules and Imports

### Package Declaration

```pgl
package main;
```

### Import

```pgl
import "stdlib/core/math" as math;    // stdlib module
import "./utils" as utils;            // relative path
```

Multi-file compilation: `pangu compile dir/` auto-discovers `.pgl` files and
merges files with the same package declaration.

### Package Management (.pgs)

Go-style package config via `pangu.pgs`:

```pgl
module "github.com/user/myapp";
require "github.com/ProtossGenius/json_pgl" v0.1.0;
replace "github.com/ProtossGenius/json_pgl" => "./vendor/github.com/ProtossGenius/json_pgl";
```

Third-party imports resolve through vendor directory with replace directives.

## Pipeline System

Pipelines are a state-machine pattern for streaming data processing (e.g., lexing).

### Declaration

```pgl
type Signal enum { CONTINUE, FINISH, TRANSFER_FINISH, APPEND, APPEND_FINISH }
type WorkerID enum { WIdent, WNumber, WString, WSymbol }
type CharToToken pipeline(c int)(kind int, text string);
```

### Switcher and Workers

```pgl
// Switcher routes each input to a worker
impl CharToToken Switcher {
    func dispatch(c int) int {
        if (c >= 'a' && c <= 'z') { return WorkerID::WIdent; }
        if (c >= '0' && c <= '9') { return WorkerID::WNumber; }
        return WorkerID::WSymbol;
    }
}

// Workers process input and return a signal
impl WIdent CharToToken worker {
    func process(c int, buf_len int, first_char int) int {
        if (c >= 'a' && c <= 'z') { return Signal::APPEND; }
        return Signal::TRANSFER_FINISH;
    }
}
```

### Auto-Generated Dispatch

When a pipeline has workers whose names match enum variants, the compiler
auto-generates `PipelineName.__dispatch(wid, ...)` which switches on worker ID:

```pgl
// Auto-generated — no need to write manually
sig := CharToToken.__dispatch(wrk, c, buf_len, first_char);
```

### Pipeline Runtime Builtins

Low-level state management for manual pipeline implementations:

| Function | Description |
|----------|-------------|
| `pipeline_create(elem_size)` | Create pipeline state |
| `pipeline_destroy(state)` | Free pipeline state |
| `pipeline_cache_append(state, char)` | Append char to buffer |
| `pipeline_cache_str(state)` | Get buffer as string, reset |
| `pipeline_cache_reset(state)` | Clear buffer |
| `pipeline_set_worker(state, id)` | Set current worker |
| `pipeline_get_worker(state)` | Get current worker |
| `pipeline_emit(state, elem)` | Store output element |
| `pipeline_output_count(state)` | Count stored outputs |
| `pipeline_output_get(state, i)` | Get stored output by index |

## Built-in Functions

### I/O

| Function | Description |
|----------|-------------|
| `println(val)` | Print with newline (auto-detects int/string) |
| `print(val)` | Print without newline |
| `read_file(path)` | Read file contents as string |
| `write_file(path, content)` | Write string to file |
| `args(index)` | Get command-line argument |
| `args_count()` | Get argument count |
| `exit(code)` | Exit with status code |
| `panic(msg)` | Print error + backtrace, abort |
| `system(cmd)` | Execute shell command |

### Strings

| Function | Description |
|----------|-------------|
| `str_concat(a, b)` | Concatenate (also `a + b`) |
| `str_len(s)` | String length |
| `str_eq(a, b)` | Equality check |
| `str_substr(s, start, end)` | Substring |
| `str_char_at(s, i)` | Char code at index |
| `char_to_str(code)` | Char code to string |
| `str_index_of(s, sub)` | Find substring |
| `str_starts_with(s, prefix)` | Prefix check |
| `str_ends_with(s, suffix)` | Suffix check |
| `str_replace(s, old, new)` | Replace substring |
| `int_to_str(n)` | Integer to string |
| `str_to_int(s)` | String to integer |

### Arrays (Fixed-size)

| Function | Description |
|----------|-------------|
| `make_array(size)` | Create int array |
| `array_get(arr, i)` | Get int element |
| `array_set(arr, i, val)` | Set int element |
| `make_str_array(size)` | Create string array |
| `str_array_get(arr, i)` | Get string element |
| `str_array_set(arr, i, val)` | Set string element |

### HashMap (string → string)

| Function | Description |
|----------|-------------|
| `make_map()` | Create empty map |
| `map_set(m, key, val)` | Set key-value pair |
| `map_get(m, key)` | Get value (empty string if missing) |
| `map_has(m, key)` | Check key exists (1/0) |
| `map_size(m)` | Number of entries |
| `map_delete(m, key)` | Remove entry |

### IntMap (string → int)

| Function | Description |
|----------|-------------|
| `make_int_map()` | Create empty int map |
| `int_map_set(m, key, val)` | Set key-value pair |
| `int_map_get(m, key)` | Get value (0 if missing) |
| `int_map_has(m, key)` | Check key exists (1/0) |
| `int_map_size(m)` | Number of entries |

### Dynamic Array (resizable int)

| Function | Description |
|----------|-------------|
| `make_dyn_array()` | Create empty dynamic array |
| `dyn_array_push(a, val)` | Append value |
| `dyn_array_get(a, i)` | Get element |
| `dyn_array_set(a, i, val)` | Set element |
| `dyn_array_size(a)` | Current size |
| `dyn_array_pop(a)` | Remove and return last |

### Dynamic String Array (resizable string)

| Function | Description |
|----------|-------------|
| `make_dyn_str_array()` | Create empty dynamic string array |
| `dyn_str_array_push(a, val)` | Append string |
| `dyn_str_array_get(a, i)` | Get string element |
| `dyn_str_array_set(a, i, val)` | Set string element |
| `dyn_str_array_size(a)` | Current size |

### String Builder

| Function | Description |
|----------|-------------|
| `make_str_builder()` | Create empty string builder |
| `sb_append(sb, str)` | Append string |
| `sb_append_int(sb, n)` | Append integer as string |
| `sb_append_char(sb, ch)` | Append char code |
| `sb_build(sb)` | Return built string |
| `sb_reset(sb)` | Clear builder |
| `sb_len(sb)` | Current length |

### File System

| Function | Description |
|----------|-------------|
| `find_pgl_files(dir, arr)` | Find .pgl files in directory |
| `is_directory(path)` | Check if path is directory |

### Reflection

| Function | Description |
|----------|-------------|
| `reflect_type_count()` | Number of registered types |
| `reflect_type_name(i)` | Type name by index |
| `reflect_field_count(type)` | Field count for type |
| `reflect_field_name(type, i)` | Field name by index |
| `reflect_field_type(type, i)` | Field type by index |
| `reflect_annotation_count(type)` | Annotation count |
| `reflect_annotation_key(type, i)` | Annotation key |
| `reflect_annotation_value(type, i)` | Annotation value |

## Semantic Analysis

The `sema` module validates before LLVM lowering:

- Function existence and argument count
- Import alias validity and imported function existence
- Variable definitions (`:=` or parameter)
- Struct type names and enum type names recognized
- Struct method calls validated (`StructName.method(args)`)

## Self-Hosting Status

**✅ Self-hosting achieved.** The bootstrap compiler in `bootstrap/` compiles
itself to produce identical output across three generations:

```bash
pangu compile bootstrap/ -o build/bootstrap
build/bootstrap compile bootstrap/ -o build/bootstrap2
build/bootstrap2 compile bootstrap/ -o build/bootstrap3
# strip bootstrap2 == strip bootstrap3  (fixed point verified)
```

The bootstrap reads PGL source, tokenizes it, parses to AST, and emits C code
that is compiled with gcc. It supports multi-file compilation, the `-o` flag,
and `--help`.

## Stream Operators

PGL provides stream operators for data-flow style programming:

### `>>` Stream Push

Appends a value to a string variable:

```pgl
buf := "";
72 >> buf;       // append char 'H' (ASCII 72) to buf
"world" >> buf;  // append string "world" to buf
println(buf);    // "Hworld"
```

### `<>` In-Place Transform

Applies a function to each character of a string, modifying it in-place:

```pgl
func to_upper(ch int) int {
    if (ch >= 97 && ch <= 122) { return ch - 32; };
    return ch;
}

s := "hello";
s <> to_upper;
println(s);  // "HELLO"
```

### `>>` Dispatch

Routes a value through predicate-based dispatch:

```pgl
c >> [is_alpha -> handle_alpha, is_digit -> handle_digit];
```

Evaluates predicates in order. The first match calls the handler with the value.
Returns the handler result, or 0 if no predicate matched.

### `==>` Pipeline Chain (Planned)

Chains pipeline stages together for end-to-end data processing.

## Closures

Closures capture variables from their enclosing scope:

```pgl
func make_adder(n int) func {
    return func(x int) int {
        return x + n;
    };
}

func main() {
    add5 := make_adder(5);
    println(add5(10));  // 15
}
```

All lambdas use the env-pointer convention: `{ ptr func, ptr env }`. Non-capturing
lambdas and function references have `env = null`.

## Generics

Generic functions use type parameters with monomorphization:

```pgl
func identity[T](x T) T {
    return x;
}

func first[T](a T, b T) T {
    return a;
}

func main() {
    println(identity(42));         // int specialization
    println(identity("hello"));   // string specialization
    println(first(10, 20));       // inferred T=int
}
```

Types are inferred from arguments. Each unique type combination generates
a specialized function.

## Interfaces

Interfaces define method contracts with vtable-based dispatch:

```pgl
type Shape interface {
    func area(self Shape) int;
    func name(self Shape) string;
}

type Rect struct {
    w int;
    h int;
}

impl Rect Shape {
    func area(self Rect) int {
        return self.w * self.h;
    }
    func name(self Rect) string {
        return "rect";
    }
}

func print_info(s Shape) {
    println(s.name());
    println(s.area());
}

func main() {
    r := Rect{w: 3, h: 4};
    s := Shape(r);       // wrap as interface fat pointer
    print_info(s);       // dispatches through vtable
}
```

Interface values are fat pointers: `{ ptr data, ptr vtable }`.
`Shape(concrete_value)` wraps a concrete type into the interface.

## Function References

Functions can be used as first-class values:

```pgl
fn := add_one;        // store function reference
println(fn(5));       // indirect call through variable
println(apply(fn, 3)); // pass as argument
```

## Planned / Not Yet Implemented

- Range patterns in match expressions
- Auto-generated pipeline `run` state machine
- Full pipeline body syntax (`type X pipeline { def in T; ... }`)
