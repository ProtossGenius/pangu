#pragma once

#include "grammer/declare.h"
#include "grammer/enums.h"
#include "lexer/datas.h"
#include "pgcodes/datas.h"
#include "pipeline/datas.h"
#include "pipeline/pipeline.h"
#include <cstddef>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
namespace pangu {
namespace grammer {
typedef pglang::INameProduct IGrammer;
class GStructContainer {
  public:
    void addStruct(PStruct &&stru);
    virtual ~GStructContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _structs.size(); }
    const std::map<std::string, PStruct> &items() const { return _structs; }
    void mergeFrom(GStructContainer &other) {
        for (auto &kv : other._structs)
            _structs[kv.first] = std::move(kv.second);
    }

  protected:
    std::map<std::string, PStruct> _structs;
};
class GFunctionContainer {
  public:
    void addFunction(PFunction &&fun);
    GFunction *getFunction(const std::string &name) {
        return _functions.count(name) ? _functions[ name ].get() : nullptr;
    }
    const GFunction *getFunction(const std::string &name) const {
        auto it = _functions.find(name);
        return it == _functions.end() ? nullptr : it->second.get();
    }
    const std::map<std::string, PFunction> &items() const { return _functions; }
    virtual ~GFunctionContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }
    void mergeFrom(GFunctionContainer &other) {
        for (auto &kv : other._functions)
            _functions[kv.first] = std::move(kv.second);
    }

  protected:
    std::map<std::string, PFunction> _functions;
};

class GTypeFunctContainer {
  public:
    void addFunction(PFuncDef &&fun);
    virtual ~GTypeFunctContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }
    const std::map<std::string, PFuncDef> &items() const { return _functions; }

  protected:
    std::map<std::string, PFuncDef> _functions;
};

class GTypeDefContainer {
  public:
    void addTypeDef(PTypeDef &&type_def);
    void write_string(std::ostream &ss);
    size_t size() const { return _type_defs.size(); }
    const std::map<std::string, PTypeDef> &items() const { return _type_defs; }

  private:
    std::map<std::string, PTypeDef> _type_defs;
};

class GVarDefContainer : public IGrammer {
  public:
    void addVariable(PVarDef &&var);
    GVarDef *getVariable(const std::string &name) {
        return _vars.count(name) ? _vars[ name ].get() : nullptr;
    }
    const GVarDef *getVariable(const std::string &name) const {
        auto it = _vars.find(name);
        return it == _vars.end() ? nullptr : it->second.get();
    }
    const std::vector<std::string> &orderedNames() const { return _ordered_names; }
    void write_string(std::ostream &ss, const std::string &splitStr);
    virtual std::string integrityTest() override { return ""; }
    virtual int         typeId() const override { return 0; }
    virtual std::string to_string() override {
        std::stringstream ss;
        ss << "[";
        write_string(ss, ",");
        ss << "]";
        return ss.str();
    }
    size_t size() const { return _vars.size(); }
    virtual ~GVarDefContainer() {}
    void swap(GVarDefContainer &rhs) {
        _vars.swap(rhs._vars);
        _no_type_vars.swap(rhs._no_type_vars);
        _ordered_names.swap(rhs._ordered_names);
    }

  protected:
    std::map<std::string, PVarDef> _vars;

  private:
    std::set<std::string> _no_type_vars;
    std::vector<std::string> _ordered_names;
};

class GImpl : public IGrammer {
  public:
    int typeId() const override { return 0; }
    void setBase(const std::string &base) { _base = base; }
    void addModifier(const std::string &modifier) { _modifiers.push_back(modifier); }
    void addBodyToken() { ++_body_token_count; }
    void setBraceDepth(int depth) { _brace_depth = depth; }
    int  getBraceDepth() const { return _brace_depth; }
    void addMethod(PFunction &&func) { _methods.push_back(std::move(func)); }
    std::vector<PFunction> &methods() { return _methods; }
    const std::string &base() const { return _base; }
    const std::vector<std::string> &modifiers() const { return _modifiers; }
    std::string to_string() override;

  private:
    std::string              _base;
    std::vector<std::string> _modifiers;
    size_t                   _body_token_count = 0;
    int                      _brace_depth      = 0;
    std::vector<PFunction>   _methods;
};

class GImplContainer {
  public:
    void addImpl(PImpl &&impl);
    void write_string(std::ostream &ss);
    size_t size() const { return _impls.size(); }
    const std::map<std::string, PImpl> &items() const { return _impls; }
    void mergeFrom(GImplContainer &other) {
        for (auto &kv : other._impls)
            _impls[kv.first] = std::move(kv.second);
    }

  private:
    std::map<std::string, PImpl> _impls;
};

// package package_name;
class GPackage : public IGrammer {
  public:
    int      typeId() const override { return EGrammer::Package; }
    void     addImport(PImport &&imp);
    GImport *getImport(const std::string &name) {
        return _imports.count(name) ? _imports[ name ].get() : nullptr;
    }
    const std::map<std::string, PImport> &imports() const { return _imports; }
    GFunction *getFunction(const std::string &name) {
        return functions.getFunction(name);
    }
    const GFunction *getFunction(const std::string &name) const {
        return functions.getFunction(name);
    }
    std::string to_string() override;

    void mergeFrom(GPackage &other) {
        functions.mergeFrom(other.functions);
        structs.mergeFrom(other.structs);
        impls.mergeFrom(other.impls);
        for (auto &kv : other._imports)
            _imports[kv.first] = std::move(kv.second);
    }

    GFunctionContainer  functions;
    GTypeFunctContainer function_defs;
    GTypeDefContainer   type_defs;
    GStructContainer    structs;
    GImplContainer      impls;
    GVarDefContainer    vars;

  private:
    std::map<std::string, PImport> _imports;
};
// import "package path" [as alias_name];
class GImport : public IGrammer {
  public:
    int                typeId() const override { return EGrammer::Import; }
    const std::string &getPackage() const { return _package; }
    void               setPackage(const std::string &pkg) { _package = pkg; }
    void               setAlias(const std::string &name) { setName(name); }
    std::string        alias() const { return name(); }
    void               setLocation(const lexer::SourceLocation &location) {
        _location = location;
    }
    const lexer::SourceLocation &location() const { return _location; }
    std::string        to_string() override;

  protected:
    std::string           _package;
    lexer::SourceLocation _location;
};
class GType : public IGrammer {
  public:
    int                typeId() const override { return 0; }
    const std::string &getPackage() { return _package; }
    void               read(std::string &str) {
        _name.swap(_package);
        _name.swap(str);
    }

    void copyFrom(const GType &rhs) {
        _name    = rhs._name;
        _package = rhs._package;
    }
    bool empty() { return _name.empty(); }

    std::string to_string() override;

  private:
    std::string _package;
};

class GVarDef : public IGrammer {
  public:
    GVarDef()
        : _type(PType(new GType())) {}
    int typeId() const override { return 0; }

  public:
    void setDetail(const std::string &detail) { _detail = detail; }
    const std::string &getDetail() { return _detail; }
    GType             *getType() { return _type.get(); }
    const GType       *getType() const { return _type.get(); }
    std::string        integrityTest() override;
    std::string        to_string() override;

    void addAnnotation(const std::string &key, const std::string &value = "") {
        _annotations.push_back({key, value});
    }
    const std::vector<std::pair<std::string, std::string>> &annotations() const {
        return _annotations;
    }

  private:
    PType       _type;
    std::string _detail;
    std::vector<std::pair<std::string, std::string>> _annotations;
};
class GTypeDef : public IGrammer {
  public:
    virtual int         typeId() const override { return 0; }
    virtual std::string to_string() override { return "typedef: " + name(); }

    void addAnnotation(const std::string &key, const std::string &value = "") {
        _annotations.push_back({key, value});
    }
    const std::vector<std::pair<std::string, std::string>> &annotations() const {
        return _annotations;
    }

  private:
    std::vector<std::pair<std::string, std::string>> _annotations;
};
class GEnum : public GTypeDef {
  public:
    void addItem(const std::string &item) { _items.push_back(item); }
    const std::vector<std::string> &items() const { return _items; }
    std::string to_string() override;

  private:
    std::vector<std::string> _items;
};
class GStruct : public GVarDefContainer {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override;

    void addAnnotation(const std::string &key, const std::string &value = "") {
        _annotations.push_back({key, value});
    }
    const std::vector<std::pair<std::string, std::string>> &annotations() const {
        return _annotations;
    }

  private:
    std::vector<std::pair<std::string, std::string>> _annotations;
};
class GInterface : public GTypeDef {
  public:
    void addBodyToken() { ++_body_token_count; }
    void setBraceDepth(int depth) { _brace_depth = depth; }
    int  getBraceDepth() const { return _brace_depth; }
    void addMethod(PFunction &&func) { _methods.push_back(std::move(func)); }
    std::vector<PFunction> &methods() { return _methods; }
    const std::vector<PFunction> &methods() const { return _methods; }
    std::string to_string() override;

  private:
    size_t _body_token_count = 0;
    int    _brace_depth      = 0;
    std::vector<PFunction> _methods;
};
class GIgnore : public IGrammer {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override { return "GIgnore"; }
};

class GFuncDef : public IGrammer {
  public:
    int                 typeId() const override { return 4; }
    std::string         sign() { return ""; }
    void                setDeclKeyword(const std::string &keyword) {
        _decl_keyword = keyword;
    }
    const std::string &getDeclKeyword() const { return _decl_keyword; }
    void                addBodyToken() { ++_body_token_count; }
    void                setBraceDepth(int depth) { _brace_depth = depth; }
    int                 getBraceDepth() const { return _brace_depth; }
    bool                hasBody() const { return _body_token_count > 0; }
    void                setLocation(const lexer::SourceLocation &loc) { _location = loc; }
    const lexer::SourceLocation &location() const { return _location; }
    void addTypeParam(const std::string &name) { _type_params.push_back(name); }
    const std::vector<std::string> &typeParams() const { return _type_params; }
    bool isGeneric() const { return !_type_params.empty(); }
    virtual std::string to_string() override;
    GVarDefContainer    params;
    GVarDefContainer    result;

  private:
    std::string _decl_keyword = "func";
    size_t      _body_token_count = 0;
    int         _brace_depth      = 0;
    lexer::SourceLocation _location;
    std::vector<std::string> _type_params;
};
using pgcodes::GCode;
using pgcodes::PCode;
class GFunction : public GFuncDef {
  public:
    PCode       code;
    std::string to_string() override;
};

} // namespace grammer
} // namespace pangu
