#pragma once

#include "grammer/declare.h"
#include "grammer/enums.h"
#include "pgcodes/datas.h"
#include "pipeline/datas.h"
#include "pipeline/pipeline.h"
#include <cstddef>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
namespace pangu {
namespace grammer {
typedef pglang::INameProduct IGrammer;
class GStructContainer {
  public:
    void addStruct(PStruct &&stru);
    virtual ~GStructContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _structs.size(); }

  protected:
    std::map<std::string, PStruct> _structs;
};
class GFunctionContainer {
  public:
    void addFunction(PFunction &&fun);
    virtual ~GFunctionContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }

  protected:
    std::map<std::string, PFunction> _functions;
};

class GTypeFunctContainer {
  public:
    void addFunction(PFuncDef &&fun);
    virtual ~GTypeFunctContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }

  protected:
    std::map<std::string, PFuncDef> _functions;
};

class GVarDefContainer : public IGrammer {
  public:
    void addVariable(PVarDef &&var);
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
    }

  protected:
    std::map<std::string, PVarDef> _vars;

  private:
    std::set<std::string> _no_type_vars;
};

// package package_name;
class GPackage : public IGrammer {
  public:
    int      typeId() const override { return EGrammer::Package; }
    void     addImport(PImport &&imp);
    GImport *getImport(const std::string &name) {
        return _imports.count(name) ? _imports[ name ].get() : nullptr;
    }
    std::string to_string() override;

    GFunctionContainer  functions;
    GTypeFunctContainer function_defs;
    GStructContainer    structs;
    GVarDefContainer    vars;

  private:
    std::map<std::string, PImport> _imports;
};
// import "package path" [as alias_name];
class GImport : public IGrammer {
  public:
    int                typeId() const override { return EGrammer::Import; }
    const std::string &getPackage() { return _package; }
    void               setPackage(const std::string &pkg) { _package = pkg; }
    void               setAlias(const std::string &name) { setName(name); }
    std::string        alias() { return name(); }
    std::string        to_string() override;

  protected:
    std::string _package;
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
    std::string        integrityTest() override;
    std::string        to_string() override;

  private:
    PType       _type;
    std::string _detail;
};
class GTypeDef : public IGrammer {
  public:
    virtual int         typeId() const override { return 0; }
    virtual std::string to_string() override { return "typedef: " + name(); }
};
class GStruct : public GVarDefContainer {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override;
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
    virtual std::string to_string() override;
    GVarDefContainer    params;
    GVarDefContainer    result;
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