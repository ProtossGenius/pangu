#include "grammer/datas.h"
#include "grammer/declare.h"
#include "pipeline/assert.h"
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
void GTypeDefContainer::addTypeDef(PTypeDef &&type_def) {
    if (_type_defs.count(type_def->name())) {
        throw std::runtime_error("existed typedef:" + type_def->name());
    }
    _type_defs[ type_def->name() ] = std::move(type_def);
}
void GTypeDefContainer::write_string(std::ostream &ss) {
    for (auto &it : _type_defs) {
        ss << it.second->to_string() << endl;
    }
}
void GVarDefContainer::addVariable(PVarDef &&var) {
    if (var->name().empty()) {
        throw std::runtime_error("varibale name() can't be empty.");
    }
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
void GVarDefContainer::write_string(std::ostream      &ss,
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
    if (type_defs.size() > 0) {
        ss << "type def list:(" << type_defs.size() << ")" << endl;
        type_defs.write_string(ss);
    }
    if (impls.size() > 0) {
        ss << "impl list:(" << impls.size() << ")" << endl;
        impls.write_string(ss);
    }
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

std::string GVarDef::integrityTest() {
    if (_name.empty()) {
        return "var need name";
    }
    if (_type->name().empty()) {
        return "var need type";
    }
    return "";
}

std::string GVarDef::to_string() {
    return "(var) " + name() + " " + _type->to_string() + " " + _detail;
}

std::string GFuncDef::to_string() {
    const std::string keyword_split = getDeclKeyword() == "func" ? "" : " ";
    return "type " + name() + keyword_split + getDeclKeyword() + "(" +
           params.to_string() + ")" +
           result.to_string();
}

std::string GEnum::to_string() {
    std::stringstream ss;
    ss << "type " << name() << " enum {";
    for (size_t i = 0; i < _items.size(); ++i) {
        if (i != 0) {
            ss << ", ";
        }
        ss << _items[ i ];
    }
    ss << "}";
    return ss.str();
}

std::string GFunction::to_string() {
    return getDeclKeyword() + " " + name() + "(" + params.to_string() + ")" +
           result.to_string() + " " +
           (code == nullptr ? "{}" : code->to_string());
}

std::string GImpl::to_string() {
    std::stringstream ss;
    ss << "impl " << name() << " " << _base;
    for (const auto &modifier : _modifiers) {
        ss << " " << modifier;
    }
    ss << " {body_tokens=" << _body_token_count << "}";
    return ss.str();
}

void GImplContainer::addImpl(PImpl &&impl) {
    if (_impls.count(impl->name())) {
        throw std::runtime_error("existed impl:" + impl->name());
    }
    _impls[ impl->name() ] = std::move(impl);
}
void GImplContainer::write_string(std::ostream &ss) {
    for (auto &it : _impls) {
        ss << it.second->to_string() << endl;
    }
}
} // namespace grammer
} // namespace pangu
