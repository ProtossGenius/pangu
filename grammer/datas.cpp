#include "grammer/datas.h"
#include "grammer/declare.h"
#include <stdexcept>
#include <utility>

namespace pangu {
namespace grammer {
void GPackage::addPackage(const std::string &name, PPackage &&pack) {
    if (_packages.count(name) == 0) {
        _packages[ name ] = std::move(pack);
        return;
    }
    _packages[ name ]->mergePackage(std::move(pack));
}
void GPackage::addStruct(const std::string &name, PStruct &&stru) {
    if (_structs.count(name)) {
        throw std::runtime_error("existed struct" + name);
    }
    _structs[ name ] = std::move(stru);
}
void GPackage::addFunction(PFunction &&fun) {
    if (_functions.count(fun->sign())) {
        throw std::runtime_error("existd function:" + fun->sign());
    }
    _functions[ fun->sign() ] = std::move(fun);
}
void GPackage::mergePackage(PPackage &&pack) {
    for (auto &packs : pack->_packages) {
        addPackage(packs.first, std::move(packs.second));
    }
    for (auto &structs : pack->_structs) {
        addStruct(structs.first, std::move(structs.second));
    }
    for (auto &func : pack->_functions) {
        addFunction(std::move(func.second));
    }
}
} // namespace grammer
} // namespace pangu