#pragma once

namespace pangu {
namespace grammer {

enum EGrammer {
    Package = 0,
    Import,
    TypeDef,  // type xxx struct {xx}
    Struct,
    Variable,
    Ignore, // ignore input.
};

}
} // namespace pangu