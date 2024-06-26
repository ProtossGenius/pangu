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

  protected:
    std::map<std::string, PStruct> _structs;
};
class GFunctionContainer {
  public:
    void addFunction(PFunction &&fun);
    virtual ~GFunctionContainer() {}

  protected:
    std::map<std::string, PFunction> _functions;
};
class GVarContainer {
  public:
    void addVariable(PVariable var) {}

  protected:
    std::map<std::string, PVariable> _vars;
};
class IGrammer : public pglang::IProduct {
  public:
    virtual ~IGrammer() {}

  protected:
    std::string name() { return _name; }

  protected:
    std::string _name;
};
// package package_name;
class GPackage : public IGrammer,
                 public virtual GStructContainer,
                 public virtual GVarContainer,
                 public virtual GFunctionContainer {
  public:
    int typeId() override { return EGrammer::Package; }
};
// import "package path" [as alias_name];
class GImport : public IGrammer {
  public:
    int typeId() override { return EGrammer::Import; }

  protected:
    GPackage   *_package;
    std::string path;
    std::string alias;
};
class GType : public IGrammer {
  public:
    int typeId() override;

    PPackage    package;
    std::string type_name;
};
class GVariable : public IGrammer {
  public:
    int         typeId() override;
    PType       type;
    std::string var_name;
};

class GStruct : public IGrammer, public virtual GVarContainer {
  public:
    std::string name() { return _name; }
    std::string _name;
    int         typeId() override;
};

class GFunction : public IGrammer {
  public:
    int           typeId() override;
    std::string   sign();
    GVarContainer params;
    GVarContainer result;
    PCode         code;
};

class GCode : public IGrammer {
  public:
    int typeId() override;

    PVariable var;
    PCode     left;
    PCode     right;
};

} // namespace grammer
} // namespace pangu