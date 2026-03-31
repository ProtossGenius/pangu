#pragma once

#include "grammer/declare.h"
#include <string>

namespace pangu {
namespace llvm_backend {

std::string emitModuleIR(const std::string &source_path);
bool emitPackageIR(const grammer::GPackage &package, const std::string &source_path,
                   std::string &ir_text, std::string &error);
bool runPackageMain(const grammer::GPackage &package,
                    const std::string      &source_path, int &exit_code,
                    std::string &error);

} // namespace llvm_backend
} // namespace pangu
