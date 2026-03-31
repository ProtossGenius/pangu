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
```

## Syntax

See `SYNTAX.md` for the current grammar, execution support matrix, and bootstrap assessment.
