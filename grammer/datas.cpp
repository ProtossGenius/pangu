#include "grammer/datas.h"
#include "grammer/declare.h"
#include <cassert>
#include <iostream>
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
    if (_functions.count(fun->name())) {
        throw std::runtime_error("existed function:" + fun->name());
    }
    _functions[ fun->name() ] = std::move(fun);
}
void GFunctionContainer::write_string(std::ostream &ss) {
    for (auto &it : _functions) {
        ss << it.second->to_string() << endl;
    }
}

void GTypeFunctContainer::addFunction(PFuncDef &&fun) {
    if (_functions.count(fun->name())) {
        throw std::runtime_error("existed function:" + fun->name());
    }
    _functions[ fun->name() ] = std::move(fun);
}
void GTypeFunctContainer::write_string(std::ostream &ss) {
    for (auto &it : _functions) {
        ss << it.second->to_string() << endl;
    }
}
void GVarContainer::addVariable(PVariable &&var) {
    if (_vars.count(var->name())) {
        throw std::runtime_error("existed varaiable : " + var->name());
    }
    if (var->getType()->empty()) {
        _no_type_vars.insert(var->name());
    } else if (!_no_type_vars.empty()) {
        for (auto name : _no_type_vars) {
            _vars[ name ]->getType()->copyFrom(*var->getType());
        }
        _no_type_vars.clear();
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

std::string GStruct::to_string() {
    std::stringstream ss;
    ss << "struct " << name() << endl;
    write_string(ss, ";\n");
    ss << endl;
    return ss.str();
}

std::string GPackage::to_string() {
    std::stringstream ss;
    ss << "package " << this->_name << endl;
    ss << "import list(" << _imports.size() << ")" << endl;
    for (auto &it : _imports) {
        ss << it.second->to_string() << endl;
    }
    ss << "struct list (" << structs.size() << "):" << endl;
    structs.write_string(ss);
    ss << "function list:(" << functions.size() << ")" << endl;
    functions.write_string(ss);
    ss << "type func list:(" << function_defs.size() << ")" << endl;
    function_defs.write_string(ss);
    ss << "variable list:(" << vars.size() << ")" << endl;
    vars.write_string(ss, "\n");
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
    return "type(" + (_package.empty() ? _name : _package + "." + name()) + ")";
}

std::string GVariable::integrityTest() {
    if (_name.empty()) {
        return "need name";
    }
    if (_type->name().empty()) {
        return "meeed type";
    }
    return "";
}

std::string GVariable::to_string() {
    return "(var) " + name() + " " + _type->to_string() + " " + _detail;
}

std::string GFuncDef::to_string() {
    return "type " + name() + "func(" + params.to_string() + ")" +
           result.to_string();
}
} // namespace grammer
} // namespace pangu