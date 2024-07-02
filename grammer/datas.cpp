#include "grammer/datas.h"
#include "grammer/declare.h"
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace pangu {
namespace grammer {
using std::endl;
void GStructContainer::addStruct(PStruct &&stru) {
    auto name = stru->name();
    if (_structs.count(name)) {
        throw std::runtime_error("existed struct" + name);
    }
    _structs[ name ] = std::move(stru);
}
void GStructContainer::write_string(std::ostream &ss) {
    for (auto &it : _structs) {
        ss << it.second->to_string() << endl;
    }
}
void GFunctionContainer::addFunction(PFunction &&fun) {
    if (_functions.count(fun->sign())) {
        throw std::runtime_error("existed function:" + fun->sign());
    }
    _functions[ fun->sign() ] = std::move(fun);
}
void GFunctionContainer::write_string(std::ostream &ss) {
    for (auto &it : _functions) {
        ss << it.second->to_string() << endl;
    }
}
void GVarContainer::addVariable(PVariable &&var) {
    if (_vars.count(var->name())) {
        throw std::runtime_error("existed varaiable : " + var->name());
    }
    _vars[ var->name() ] = std::move(var);
}
void GVarContainer::write_string(std::ostream      &ss,
                                 const std::string &splitStr) {
    bool first = true;
    for (auto &it : _vars) {
        if (!first) {
            ss << splitStr;
        }
        first = false;
        ss << it.second->to_string();
    }
}

std::string GPackage::to_string() {
    std::stringstream ss;
    ss << "package " << this->_name << endl;
    ss << "import list(" << _imports.size() << ")" << endl;
    for (auto &it : _imports) {
        ss << it.second->to_string() << endl;
    }
    ss << "struct list (" << _structs.size() << "):" << endl;
    GStructContainer::write_string(ss);
    ss << "function list:(" << _functions.size() << ")" << endl;
    GFunctionContainer::write_string(ss);
    ss << "variable list:(" << _vars.size() << ")" << endl;
    GVarContainer::write_string(ss, "\n");
    return ss.str();
}
void GPackage::addImport(PImport &&imp) {
    if (_imports.count(imp->name())) {
        throw std::runtime_error("existed import : " + imp->name());
    }
    _imports[ imp->name() ] = std::move(imp);
}

std::string GImport::to_string() {
    return "import " + _package + " as " + name();
}

std::string GType::to_string() {
    return _package.empty() ? _name : _package + "." + name();
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

std::string GVariable::to_string() {
    return name() + " " + _type->to_string() + " " + _detail;
}
} // namespace grammer
} // namespace pangu