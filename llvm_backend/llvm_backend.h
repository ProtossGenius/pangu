#pragma once

#include "grammer/declare.h"
#include <map>
#include <string>
#include <vector>

namespace pangu {
namespace llvm_backend {

struct PackageUnit {
    std::string                                  module_id;
    std::string                                  source_path;
    const grammer::GPackage                     *package = nullptr;
    std::map<std::string, std::string>           import_alias_to_module;
};

struct Program {
    std::string            entry_module_id;
    std::vector<PackageUnit> packages;
};

std::string emitModuleIR(const std::string &source_path);
bool emitProgramIR(const Program &program, const std::string &source_path,
                   std::string &ir_text, std::string &error);
bool runProgramMain(const Program &program,
                    const std::string &source_path, int &exit_code,
                    std::string &error,
                    int argc = 0, const char *argv[] = nullptr);
bool compileProgramToExecutable(const Program      &program,
                                const std::string      &source_path,
                                const std::string      &output_path,
                                std::string            &error);

} // namespace llvm_backend
} // namespace pangu
