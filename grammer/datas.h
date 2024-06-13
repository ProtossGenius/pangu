#pragma once
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "pipeline/pipeline.h"
#include <map>
#include <string>
#include <vector>
namespace pangu {
namespace grammer {

class GPackage : public pglang::IProduct {
  public:
    int typeId() override { return EGrammer::Package; }

  public:
    void addPackage(const std::string &name, PPackage &&pack);
    void addStruct(const std::string &name, PStruct &&stru);
    void addFunction(PFunction &&fun);

  protected:
    void setParent(GPackage *parent) { _parent = parent; }

  private:
    void mergePackage(PPackage &&pack);

  private:
    std::map<std::string, PPackage>  _packages;
    std::map<std::string, PStruct>   _structs;
    std::map<std::string, PFunction> _functions;
    GPackage                        *_parent;
};
class GType : public pglang::IProduct {
  public:
    int typeId() override;

    PPackage    package;
    std::string type_name;
};
class GVariable : public pglang::IProduct {
  public:
    int         typeId() override;
    PType       type;
    std::string var_name;
};

class GStruct : public pglang::IProduct {
  public:
    int                    typeId() override;
    std::vector<PVariable> variables;
    std::vector<PStruct>   structs;
};

class GFunction : public pglang::IProduct {
  public:
    int                    typeId() override;
    std::string            sign();
    std::vector<PVariable> params;
    std::vector<PVariable> result;
    PCode                  code;
};

class GCode : public pglang::IProduct {
  public:
    int typeId() override;

    PVariable var;
    PCode     left;
    PCode     right;
};

} // namespace grammer
} // namespace pangu