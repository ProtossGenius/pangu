#pragma once
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "pipeline/pipeline.h"
#include <map>
#include <string>
#include <vector>
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

class IGrammer : public pglang::IProduct {
  public:
    virtual ~IGrammer() {}

  protected:
    std::string name() { return _name; }

  protected:
    std::string _name;
};

class GPackage : public IGrammer,
                 public virtual GStructContainer,
                 public virtual GFunctionContainer {
  public:
    int typeId() override { return EGrammer::Package; }
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

class GStruct : public IGrammer {
  public:
    std::string            name() { return _name; }
    std::string            _name;
    int                    typeId() override;
    std::vector<PVariable> variables;
};

class GFunction : public IGrammer {
  public:
    int                    typeId() override;
    std::string            sign();
    std::vector<PVariable> params;
    std::vector<PVariable> result;
    PCode                  code;
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