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
- integer parameters and a single integer return value
- integer variables
- integer arithmetic `+ - * /`
- integer comparison `== != > < >= <=`
- assignment / define-assignment
- user-defined function calls
- imported function calls through `alias.func(...)`
- `println(<int-expr>)`
- `if / else if / else`
- `return`

### Direct-run support

Supported today by `./build/pangu run`:

- recursive import loading for `stdlib/...` and relative module paths
- multiple top-level functions
- integer parameters and a single integer return value
- integer locals
- integer arithmetic
- integer comparisons
- user-defined function calls
- imported function calls through `alias.func(...)`
- `if / else if / else`
- `println`
- `return`

For backend-stable code today, using a temporary variable before `return` is safer than returning a compound expression directly.

### Native compile support

Supported today by `./build/pangu compile`:

- resolve imported modules before lowering
- emit LLVM IR for the supported runnable subset
- include integer comparisons and `if / else if / else` lowering in that subset
- support imported function calls through `alias.func(...)`
- invoke system clang to produce a native executable
- place the executable at `build/<source-stem>`

### Planned but not implemented end-to-end

- full type checking (types are parsed but not verified)
- structs, enum semantics, and impl semantics in codegen
- loop lowering
- switch lowering
- parser-only interface declarations
- case expressions used in design files
- full pipeline runtime semantics

### Semantic analysis

The `sema` module now performs the following checks before LLVM lowering:

- **Function existence:** calls to undefined functions are reported.
- **Argument count:** mismatch between actual and formal parameter count is reported.
- **Import alias validity:** `alias.func()` calls with unknown aliases are reported.
- **Imported function existence:** calling a function that does not exist in the imported module is reported.
- **Variable definitions:** use of undefined variables (not declared with `:=` or as parameters) is reported.

Errors are printed in the format:

```text
/path/to/file.pgl: error: in function 'main': undefined function 'foo'
```

## Bootstrap assessment

## Conclusion

Pangu is **not yet self-hostable**.

### Why it is not ready to bootstrap

1. The front-end accepts more syntax than the backend can execute.
2. `sema/` now checks function existence, argument counts, import aliases, and variable definitions, but does not yet perform type checking or method binding.
3. The backend still executes only a narrow integer-oriented subset, even though integer control flow is now available.
4. Imports and package linking now work for basic function-oriented modules.
5. Important language features used by the design files are still parser-only or still planned:
   - enum semantics
   - impl body semantics
   - interface declarations
   - richer pipeline definitions
6. A standard library directory now exists and participates in basic imports, but it is still a minimal scaffold rather than a bootstrap-ready library surface.

### What would be required before self-hosting

1. Complete semantic analysis:
   - type checking (argument types, return types)
   - enum/struct/interface checking
   - method binding and resolution

2. Complete backend:
   - full expression lowering
   - loops and switch lowering
   - aggregate values
    - richer package/import linking beyond basic function calls

3. Runtime and library support:
   - basic IO
   - string/runtime conventions
    - richer import/module loading semantics
   - deterministic build entrypoint

4. Front-end completion for design syntax:
   - richer `impl`
   - `interface`
   - full `pipeline` meta-definition syntax
   - `case` forms shown in `examples/pglang/main.pgl`

### Practical assessment

- **Parser self-description:** partially feasible now.
- **Compiler implementation in Pangu:** not yet feasible.
- **Real bootstrap chain:** not yet feasible.

The most realistic near-term milestone is:

1. finish `sema`,
2. lower loops, switch, and aggregates through LLVM,
3. extend `stdlib/` and import handling beyond the current basic function-call model,
4. expand the standard library until the compiler can depend on it,
5. then reassess bootstrap.
