#pragma once

#include <string>

namespace pangu {
namespace llvm_backend {

std::string emitModuleIR(const std::string &source_path);

} // namespace llvm_backend
} // namespace pangu
