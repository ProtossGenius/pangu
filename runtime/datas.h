#pragma once
#include "grammer/datas.h"
#include <map>
#include <string>
namespace pangu {
namespace runtime {
class VarTable {
  public:
  private:
    std::map<std::string, grammer::GPackage *> _package_map;
    std::map<std::string, grammer::GStruct *>  _struct_map;
};
class Runner {
  public:
  private:
};
} // namespace runtime
} // namespace pangu