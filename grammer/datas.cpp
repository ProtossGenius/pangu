#include "grammer/datas.h"
#include "grammer/declare.h"
#include <stdexcept>
#include <utility>

namespace pangu {
namespace grammer {
void GStructContainer::addStruct(PStruct &&stru) {
    auto name = stru->name();
    if (_structs.count(name)) {
        throw std::runtime_error("existed struct" + name);
    }
    _structs[ name ] = std::move(stru);
}
void GFunctionContainer::addFunction(PFunction &&fun) {
    if (_functions.count(fun->sign())) {
        throw std::runtime_error("existd function:" + fun->sign());
    }
    _functions[ fun->sign() ] = std::move(fun);
}
} // namespace grammer
} // namespace pangu