# Pangu Syntax and Execution Model

## Status

This document describes the current Pangu language surface in this repository.

- `Implemented` means the syntax is accepted by the current front-end.
- `Runnable` means the current LLVM direct-run path can execute it.
- `Planned` means the syntax appears in `.pgl` design files or comments, but is not fully implemented yet.

## Design goal

Pangu is intended to make program structure dense and readable:

- top-level files describe module structure first,
- mid-level files describe pipeline and type structure,
- low-level files hold concrete algorithms.

The repository currently provides:

- a lexer,
- a grammar/parser,
- a partial code tree builder,
- a minimal LLVM IR backend,
- a minimal LLVM JIT direct-run path,
- a bootstrap-oriented standard library scaffold in `stdlib/`.

## Toolchain modes

```bash
make linux
make tests

./build/pangu parse   <file.pgl>
./build/pangu emit-ir <file.pgl>
./build/pangu run     <file.pgl>
./build/pangu compile <file.pgl>
```

`compile` currently writes the executable to `build/<source-stem>`.

Syntax diagnostics now use a clang/LLVM-style format:

```text
path/file.pgl:line:column: error: message
<source line>
^
```

## Lexical structure

### Comments

The current lexer accepts comment forms used in the repository examples:

- line comments: `// ...`
- block comments: `/* ... */`

### Strings and numbers

- string literals are accepted by the lexer and parser
- integer numbers are accepted and used by the current LLVM run path
- float syntax appears in samples, but is not runnable in the current backend

### Keywords

Current keyword set:

`package`, `import`, `as`, `type`, `struct`, `enum`, `func`, `pipeline`, `impl`, `if`, `else`, `for`, `while`, `do`, `return`, `switch`, `goto`, `try`, `catch`, `public`, `static`, `const`, `final`, `var`, `class`, `switcher`, `worker`

### Important symbols

The lexer recognizes symbols used by the current parser/code tree, including:

`(` `)` `[` `]` `{` `}` `,` `;` `.` `:` `::` `:=` `?` `->`

Arithmetic and comparison operators recognized by the lexer include:

`+` `-` `*` `/` `%` `++` `--` `=` `==` `!=` `>` `<` `>=` `<=` `&&` `||`

## Grammar

The following grammar is a practical description of the current parser, not a claim that every production is fully semantic-checked.

### Source file

```ebnf
source-file = package-decl top-level-decl* EOF ;

top-level-decl =
    import-decl
  | type-struct-decl
  | type-enum-decl
  | type-func-decl
  | type-pipeline-decl
  | func-decl
  | pipeline-decl
  | impl-decl
  | ignored-token ;
```

### Package and import

```ebnf
package-decl = "package" identifier ";" ;
import-decl  = "import" string-literal [ "as" identifier ] ";" ;
```

Examples:

```pgl
package main;
import "llvm" as llvm;
```

### Struct type

```ebnf
type-struct-decl =
  "type" identifier "struct" "{" field-decl* "}" ;

field-decl =
  identifier-list type-ref [ string-literal ] [ ";" ] ;
```

Examples:

```pgl
type Test struct {
  a int;
  b int;
  c int `json: 'cat'`;
}
```

### Enum type

Implemented in this repository.

```ebnf
type-enum-decl =
  "type" identifier "enum" "{" enum-item ( "," enum-item )* [ "," ] "}" ;

enum-item = identifier ;
```

Example:

```pgl
type LexType enum {
  STRING,
  CHAR,
  NUMBER,
}
```

### Function type and pipeline type declaration

Implemented as declaration syntax, and `type ... pipeline` now also has a parser-only body form.

```ebnf
type-func-decl =
  "type" identifier "func" param-list [ result-list ] ";" ;

type-pipeline-decl =
  "type" identifier "pipeline" param-list [ result-list ] ( ";" | raw-brace-block ) ;
```

Examples:

```pgl
type TestFunc1 func (a int, b int, c string) int ;
type PLexer pipeline(c char)(o Lex);
type PipeSelf pipeline(i In)(o Out) { ... }
```

Note:

- `type X func (...) ... { ... }` is still rejected with a readable parse error.

### Interface type

Implemented as parser-only body capture.

```ebnf
type-interface-decl =
  "type" identifier "interface" raw-brace-block ;
```

Example:

```pgl
type Reader interface {
  Accept(test Test) (bool, error);
}
```

### Top-level function and top-level pipeline

Implemented in the parser.

```ebnf
func-decl =
  "func" identifier param-list [ result-list ] code-block ;

pipeline-decl =
  "pipeline" identifier param-list [ result-list ] code-block ;
```

Examples:

```pgl
func main() {
  println(1 + 2);
}

pipeline Build(cfg Config) (out Reader) {
  PipeReader -> PipeNormalize;
}
```

### Implementation block

Top-level `impl` blocks are now parsed.

```ebnf
impl-decl =
  "impl" identifier identifier modifier* raw-brace-block ;

modifier = identifier ;
```

Examples:

```pgl
impl Lexer Switcher {
  []
  func create() Lex { return Lex{}; }
}

impl PLexNumber PLexer worker {
  [AFTER_POINT]
}
```

Current implementation detail:

- the parser recognizes the `impl` header and consumes the whole brace-balanced body,
- the current AST stores the body as a summarized token count,
- inner `impl` members are not yet semantically analyzed.

### Parameter and result lists

```ebnf
param-list  = "(" [ variable-decl ( "," variable-decl )* ] ")" ;
result-list = param-list | type-ref ;
```

Examples:

```pgl
func hello(a int) {}
func f(a int, b string) (ok bool, err error) {}
```

### Expressions and statements

The current code-tree parser supports a broad expression syntax used by existing tests:

- assignment: `=`, `:=`
- arithmetic: `+`, `-`, `*`, `/`
- integer comparison: `==`, `!=`, `>`, `<`, `>=`, `<=`
- precedence and parentheses
- prefix/postfix `++`, `--`
- function call form: `f(x, y)`
- index form: `a[i]`
- ternary form in samples: `cond ? a : b`
- `if / else if / else`
- `while`
- `for`
- `switch(expr) { ... }`
- `return`
- brace blocks

Examples from current tests:

```pgl
a := 1;
b = 3 * 4 * (1 + 5);
println(a + b);
if (a == 1) return;
if (a > 1) { return 2; } else { return 3; }
while (a < 3) { a = a + 1; }
for (i := 0; i < 2; ++i) { println(i); }
switch (a) {}
```

## Execution support matrix

### Parser support

Supported by `./build/pangu parse`:

- `package`
- `import`
- `type ... struct`
- `type ... enum`
- `type ... func`
- `type ... pipeline` declaration and parser-only body form
- `type ... interface`
- top-level `func`
- top-level `pipeline`
- top-level `impl`
- arithmetic / comparison / assignment / call / `if` / `else if` / `else` / `while` / `for` / `switch` / `return` code trees

### LLVM IR emission

Supported today by `./build/pangu emit-ir`:

- source parsing through the grammar front-end
- recursive import loading for `stdlib/...` and relative module paths
- generation of multiple top-level functions
- integer and string parameters, single return value
- integer and string variables (type-inferred via `:=`)
- integer arithmetic `+ - * / %`
- integer comparison `== != > < >= <=`
- logical operators `&& || !` (short-circuit)
- prefix `++` / `--`
- assignment / define-assignment
- user-defined function calls
- imported function calls through `alias.func(...)`
- `println(<expr>)` — auto-detects int (%d) vs string (%s)
- `print(<expr>)` — same as println without trailing newline
- `exit(<int>)` — terminates with exit code
- `if / else if / else`
- `while (cond) { ... }`
- `for (init; cond; step) { ... }`
- `return`
- string literals with escape sequences (`\n`, `\t`, `\\`, `\"`)

### Direct-run support

Supported today by `./build/pangu run`:

- all features listed in LLVM IR emission above
- uses LLVM ORC JIT for in-process execution

### Native compile support

Supported today by `./build/pangu compile`:

- all features listed in LLVM IR emission above
- invokes system clang to produce a native executable
- places the executable at `build/<source-stem>`

### Planned but not implemented end-to-end

- full type checking (types are parsed but not verified)
- structs, enum semantics, and impl semantics in codegen
- switch lowering
- break / continue in loops
- parser-only interface declarations
- case expressions used in design files
- full pipeline runtime semantics
- array / slice types
- string concatenation

### Semantic analysis

The `sema` module now performs the following checks before LLVM lowering:

- **Function existence:** calls to undefined functions are reported.
- **Argument count:** mismatch between actual and formal parameter count is reported.
- **Import alias validity:** `alias.func()` calls with unknown aliases are reported.
- **Imported function existence:** calling a function that does not exist in the imported module is reported.
- **Variable definitions:** use of undefined variables (not declared with `:=` or as parameters) is reported.

All errors use clang-style diagnostics with file:line:column, source line, and caret:

```text
/path/to/file.pgl:4:13: error: undefined function 'not_exist'
    println(not_exist(1));
            ^~~~~~~~~
```

## Bootstrap assessment

## Conclusion

Pangu is **approaching self-hostability**. Most foundational capabilities are in place.

### Current capabilities

1. **Control flow:** if/else, while, for loops all work end-to-end (parse → sema → LLVM → run/compile).
2. **Types:** int, string, and user-defined structs are supported.
3. **Operators:** arithmetic (+, -, *, /, %), comparison (==, !=, >, <, >=, <=), logical (&&, ||, !), increment (++, --). String operands auto-dispatch to strcmp/concat.
4. **Strings:** literals with escapes, concatenation (`+`), length, substring, comparison, char access, int↔string conversion.
5. **Structs:** definition, construction (`Point{x: 1, y: 2}`), field access (`p.x`), pass as function params and return values.
6. **Arrays:** int arrays (`make_array`, `array_get`, `array_set`) and string arrays (`make_str_array`, `str_array_get`, `str_array_set`).
7. **File I/O:** `read_file(path)` and `write_file(path, content)`.
8. **Character ops:** `str_char_at(s, i)` and `char_to_str(code)`.
9. **Modules:** import system works for stdlib and relative paths with multi-package compilation.
10. **Sema:** function/variable/import/struct validation with clang-style diagnostics.
11. **Builtins:** println, print, exit, plus all string/array/file ops above.

### What is still missing for self-hosting

1. **Type system gaps:**
   - type checking in sema (argument types, return types)
   - enum values and pattern matching

2. **Control flow gaps:**
   - switch/case lowering (parser needs `case:` support)
   - break/continue in loops

3. **Advanced features:**
   - struct methods (impl blocks)
   - interface dispatch
   - closures or function values
   - command-line argument access

### Practical assessment

- **Simple programs:** fully feasible — arithmetic, control flow, strings, structs, arrays, modules all work.
- **Text processing utility:** fully feasible — string ops, file IO, arrays all available.
- **Simple compiler (lexer + parser):** feasible — can read files, iterate chars, build token arrays, construct AST with structs, generate output. Missing switch/case (use if/else chains instead).
- **Full bootstrap chain:** close but needs break/continue, command-line args, and more robust error handling.

### Nearest milestones toward bootstrap

1. ~~Add string operations~~ ✅
2. ~~Add struct definition + field access~~ ✅
3. ~~Add array/slice type with indexing~~ ✅
4. ~~Add file IO builtins~~ ✅
5. Add switch/case lowering (requires parser extension for `case:`)
6. Add break/continue support in loops
7. Add command-line argument access (`args()` builtin)
8. Write a minimal PGL lexer in PGL itself as proof-of-concept
