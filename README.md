# Pipeline Go language.
a programming language. base Golang grammer, and add some featuer to improve readable.

developing...

## Build

```bash
make linux
```

## Test

```bash
make tests
```

## Driver modes

```bash
./build/pangu parse test_datas/grammer/func_code.pgl
./build/pangu emit-ir test_datas/runtime/add.pgl
./build/pangu run test_datas/runtime/add.pgl
./build/pangu compile test_datas/compile/call_func.pgl
./build/call_func
```

`compile` currently writes the executable to `build/<source-stem>`.

## Standard library

Bootstrap-oriented PGL standard library files now live under `stdlib/`.

- `stdlib/prelude.pgl`
- `stdlib/core/io.pgl`
- `stdlib/core/math.pgl`

## Syntax

See `SYNTAX.md` for the current grammar, execution support matrix, and bootstrap assessment.
