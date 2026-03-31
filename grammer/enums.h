#pragma once

namespace pangu {
namespace grammer {

enum EGrammer {
    Package = 0, // package ...;
    Import,      // import ... as ...;
    TypeDef,     // type xxx struct {xx}
    Struct,      // type xxx struct {xx}
    Enum,        // type xxx enum {A, B}
    Variable,    // pkg.type name
    Interface,   // type X interface {...}
    Impl,        // impl Foo Bar {...}
    Ignore,      // ignore input.
    VarArray,    // for func ( --var array-- )
    TypeFunc,    // type func(...) ...
    Func,        // func name(...) (...)
    CodeBlock,   // {code...}
};

}
} // namespace pangu
