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
        throw std::runtime_error("existed function:" + fun->sign());
    }
    _functions[ fun->sign() ] = std::move(fun);
}
void GVarContainer::addVariable(PVariable &&var) {
    if (_vars.count(var->name())) {
        throw std::runtime_error("existed varaiable : " + var->name());
    }
    _vars[ var->name() ] = std::move(var);
}

void GPackage::addImport(PImport &&imp) {
    if (_imports.count(imp->name())) {
        throw std::runtime_error("existed import : " + imp->name());
    }
    _imports[ imp->name() ] = std::move(imp);
}
std::string GVariable::integrityTest() {
    if (_name.empty()) {
        return "need name";
    }
    if (_type->getName().empty()) {
        return "need type";
    }
    return "";
}
} // namespace grammer
} // namespace pangu