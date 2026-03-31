#include "llvm_backend/llvm_backend.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

namespace pangu {
namespace llvm_backend {
namespace {

std::string moduleNameFromPath(const std::string &source_path) {
    const std::string::size_type slash_pos = source_path.find_last_of("/\\");
    const std::string            file_name =
        slash_pos == std::string::npos ? source_path
                                       : source_path.substr(slash_pos + 1);
    const std::string::size_type dot_pos = file_name.find_last_of('.');
    return dot_pos == std::string::npos ? file_name
                                        : file_name.substr(0, dot_pos);
}

} // namespace

std::string emitModuleIR(const std::string &source_path) {
    llvm::LLVMContext context;
    llvm::Module      module(moduleNameFromPath(source_path), context);
    module.setSourceFileName(source_path);

    llvm::IRBuilder<> builder(context);
    auto             *main_type = llvm::FunctionType::get(
        builder.getInt32Ty(), false);
    auto *main_func = llvm::Function::Create(main_type,
                                             llvm::Function::ExternalLinkage,
                                             "main", module);
    auto *entry = llvm::BasicBlock::Create(context, "entry", main_func);
    builder.SetInsertPoint(entry);
    builder.CreateRet(llvm::ConstantInt::get(builder.getInt32Ty(), 0));

    std::string              ir_text;
    llvm::raw_string_ostream os(ir_text);
    module.print(os, nullptr);
    return ir_text;
}

} // namespace llvm_backend
} // namespace pangu
