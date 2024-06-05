#pragma once
#include "grammer/declare.h"
#include "pipeline/pipeline.h"
#include <map>
#include <string>
#include <vector>
namespace pangu {
namespace grammer {

class GPackage : public pglang::IData {
  public:
    int typeId() override;

  private:
    std::map<std::string, PPackage> packages;
    std::map<std::string, PStruct>  structs;
    std::vector<PFunction>          functions;
    GPackage                       *parent;
};
class GType : public pglang::IData {
  public:
    int typeId() override;

    PPackage    package;
    std::string type_name;
};
class GVariable : public pglang::IData {
  public:
    int         typeId() override;
    PType       type;
    std::string var_name;
};

class Gtruct : public pglang::IData {
  public:
    int                    typeId() override;
    std::vector<PVariable> variables;
    std::vector<PStruct>   structs;
};

class GFunction : public pglang::IData {
  public:
    int typeId() override;

    std::vector<PVariable> params;
    std::vector<PVariable> result;
    PCode                  code;
};

class GCode : public pglang::IData {
  public:
    int typeId() override;

    PVariable var;
    PCode     left;
    PCode     right;
};

} // namespace grammer
} // namespace pangu