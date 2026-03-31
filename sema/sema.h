#pragma once

#include "llvm_backend/llvm_backend.h"
#include <string>
#include <vector>

namespace pangu {
namespace sema {

struct Diagnostic {
    std::string message;
};

struct CheckResult {
    bool                     ok = true;
    std::vector<Diagnostic>  errors;

    void addError(const std::string &message) {
        ok = false;
        errors.push_back({message});
    }
};

CheckResult checkProgram(const llvm_backend::Program &program);

} // namespace sema
} // namespace pangu
