#pragma once

namespace pangu {
namespace grammer {

enum EGrammer {
    Package = 0, // package ...;
    Import,      // import ... as ...;
    TypeDef,     // type xxx struct {xx}
    Struct,      // type xxx struct {xx}
    Variable,    // pkg.type name
    Ignore,      // ignore input.
    VarArray,    // for func ( --var array-- )
    TypeFunc,    // type func(...) ...
};

}
} // namespace pangu