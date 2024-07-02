#pragma once
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "pipeline/pipeline.h"
#include <map>
#include <string>
namespace pangu {
namespace grammer {

class GStructContainer {
  public:
    void addStruct(PStruct &&stru);
    virtual ~GStructContainer() {}
    void write_string(std::ostream &ss);

  protected:
    std::map<std::string, PStruct> _structs;
};
class GFunctionContainer {
  public:
    void addFunction(PFunction &&fun);
    virtual ~GFunctionContainer() {}
    void write_string(std::ostream &ss);

  protected:
    std::map<std::string, PFunction> _functions;
};
class GVarContainer {
  public:
    void addVariable(PVariable &&var);
    void write_string(std::ostream &ss, const std::string &splitStr);

  protected:
    std::map<std::string, PVariable> _vars;
};
class IGrammer : public pglang::IProduct {
  public:
    virtual std::string integrityTest() { return ""; }
    virtual int         typeId() const override = 0;
    virtual std::string to_string() override { return ""; }
    virtual ~IGrammer() {}

  public:
    std::string name() { return _name; }
    void        setName(const std::string &name) { _name = name; }

  protected:
    std::string _name;
};

class GStep {
  public:
    GStep()
        : _step(0) {}
    virtual ~GStep() {}
    int  getStep() { return _step; }
    void setStep(int step) { this->_step = step; }

  private:
    int _step;
};

// package package_name;
class GPackage : public IGrammer,
                 public GStep,
                 public virtual GStructContainer,
                 public virtual GVarContainer,
                 public virtual GFunctionContainer {
  public:
    int      typeId() const override { return EGrammer::Package; }
    void     addImport(PImport &&imp);
    GImport *getImport(const std::string &name) {
        return _imports.count(name) ? _imports[ name ].get() : nullptr;
    }
    std::string to_string() override;

  private:
    std::map<std::string, PImport> _imports;
};
// import "package path" [as alias_name];
class GImport : public IGrammer, public GStep {
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
    int                typeId() const override;
    const std::string &getPackage() { return _package; }
    void               read(std::string &str) {
                      _name.swap(_package);
                      _name.swap(str);
    }

    std::string to_string() override;

  private:
    std::string _package;
};

class GVariable : public IGrammer, public GStep {
  public:
    GVariable()
        : _type(PType(new GType())) {}
    int typeId() const override;

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

class GStruct : public IGrammer, public GVarContainer, public GStep {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override;
};

class GFunction : public IGrammer {
  public:
    int           typeId() const override { return 4; }
    std::string   sign() { return ""; }
    GVarContainer params;
    GVarContainer result;
    PCode         code;
};

class GCode : public IGrammer {
  public:
    int typeId() const override;

    PVariable var;
    PCode     left;
    PCode     right;
};

} // namespace grammer
} // namespace pangu