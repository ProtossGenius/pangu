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
    int typeId();

  private:
    std::vector<PPackage>          packages;
    std::map<std::string, PStruct> structs;
    std::vector<PFunction>         functions;
    GPackage                      *parent;
};
class GType {
    PPackage    package;
    std::string type_name;
};
class GVariable {
    PType       type;
    std::string var_name;
};

class Gtruct {
    std::vector<PVariable> variables;
    std::vector<PStruct>   structs;
};

class GFunction {
    std::vector<PVariable> params;
    std::vector<PVariable> result;
    PCode                  code;
};

class GCode {
    PVariable var;
    PCode     left;
    PCode     right;
};

} // namespace grammer
} // namespace pangu