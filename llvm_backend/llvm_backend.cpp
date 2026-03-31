#include "llvm_backend/llvm_backend.h"

#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>

#include <map>
#include <memory>
#include <stdexcept>

#include "grammer/datas.h"
#include "pgcodes/datas.h"

#ifdef T
#undef T
#endif

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

class ModuleBuilder {
  public:
    ModuleBuilder(const grammer::GPackage &package,
                  const std::string       &source_path)
        : _context(new llvm::LLVMContext())
        , _module(new llvm::Module(moduleNameFromPath(source_path), *_context))
        , _builder(*_context)
        , _package(package) {
        _module->setSourceFileName(source_path);
    }

    std::unique_ptr<llvm::LLVMContext> releaseContext() {
        return std::move(_context);
    }

    std::unique_ptr<llvm::Module> releaseModule() { return std::move(_module); }

    void buildMain() {
        const auto *main_code = _package.getFunction("main");
        if (main_code == nullptr) {
            throw std::runtime_error("main function not found");
        }
        auto *main_type = llvm::FunctionType::get(_builder.getInt32Ty(), false);
        _main_function  = llvm::Function::Create(
            main_type, llvm::Function::ExternalLinkage, "main", *_module);
        auto *entry =
            llvm::BasicBlock::Create(*_context, "entry", _main_function);
        _builder.SetInsertPoint(entry);
        emitStatement(main_code->code.get());
        if (!_terminated) {
            _builder.CreateRet(llvm::ConstantInt::get(_builder.getInt32Ty(), 0));
        }
    }

  private:
    llvm::Value *emitStatement(const pgcodes::GCode *code) {
        if (code == nullptr || _terminated) {
            return nullptr;
        }
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            return emitExpression(code);
        }

        const std::string oper = code->getOper();
        if (oper == "{") {
            return emitStatement(code->getRight());
        }
        if (oper == ";") {
            emitStatement(code->getLeft());
            return emitStatement(code->getRight());
        }
        return emitExpression(code);
    }

    llvm::Value *emitExpression(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            return emitValue(code);
        }

        const std::string oper = code->getOper();
        if (oper == ":=" || oper == "=") {
            return emitAssignment(code, oper == ":=");
        }
        if (oper == "+") {
            return _builder.CreateAdd(emitExpression(code->getLeft()),
                                      emitExpression(code->getRight()), "addtmp");
        }
        if (oper == "-") {
            return _builder.CreateSub(emitExpression(code->getLeft()),
                                      emitExpression(code->getRight()), "subtmp");
        }
        if (oper == "*") {
            return _builder.CreateMul(emitExpression(code->getLeft()),
                                      emitExpression(code->getRight()), "multmp");
        }
        if (oper == "/") {
            return _builder.CreateSDiv(emitExpression(code->getLeft()),
                                       emitExpression(code->getRight()),
                                       "divtmp");
        }
        if (oper == "(") {
            return emitParenOrCall(code);
        }
        if (oper == "{") {
            emitStatement(code);
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        throw std::runtime_error("unsupported operator in LLVM backend: " +
                                 oper);
    }

    llvm::Value *emitValue(const pgcodes::GCode *code) {
        switch (code->getValueType()) {
        case pgcodes::ValueType::NUMBER:
            return llvm::ConstantInt::get(_builder.getInt32Ty(),
                                          std::stoi(code->getValue()));
        case pgcodes::ValueType::IDENTIFIER: {
            auto it = _variables.find(code->getValue());
            if (it == _variables.end()) {
                throw std::runtime_error("unknown identifier: " +
                                         code->getValue());
            }
            return _builder.CreateLoad(_builder.getInt32Ty(), it->second,
                                       code->getValue());
        }
        case pgcodes::ValueType::STRING:
            throw std::runtime_error("string is not supported in LLVM backend");
        case pgcodes::ValueType::NOT_VALUE: break;
        }
        throw std::runtime_error("unexpected empty value");
    }

    llvm::Value *emitAssignment(const pgcodes::GCode *code, bool define_new) {
        const auto *left = code->getLeft();
        if (left == nullptr ||
            left->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("assignment left value must be identifier");
        }

        const std::string &name = left->getValue();
        llvm::AllocaInst  *slot = nullptr;
        auto               it   = _variables.find(name);
        if (it == _variables.end()) {
            if (!define_new) {
                throw std::runtime_error("assign to undefined identifier: " +
                                         name);
            }
            slot             = createVariableSlot(name);
            _variables[name] = slot;
        } else {
            slot = it->second;
        }

        auto *value = emitExpression(code->getRight());
        _builder.CreateStore(value, slot);
        return value;
    }

    llvm::Value *emitParenOrCall(const pgcodes::GCode *code) {
        const auto *left = code->getLeft();
        if (left == nullptr) {
            throw std::runtime_error("call expression misses callee");
        }
        if (left->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string callee = left->getValue();
            if (callee == "println") {
                auto *value = emitExpression(code->getRight());
                auto *fmt   = _builder.CreateGlobalStringPtr("%d\n");
                _builder.CreateCall(getPrintf(), {fmt, value});
                return value;
            }
            if (callee == "return") {
                auto *value = code->getRight() == nullptr
                                  ? llvm::ConstantInt::get(_builder.getInt32Ty(),
                                                           0)
                                  : emitExpression(code->getRight());
                _builder.CreateRet(value);
                _terminated = true;
                return value;
            }
            throw std::runtime_error("unsupported function call: " + callee);
        }
        if (left->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            left->getOper() == ")") {
            return emitExpression(code->getRight());
        }
        throw std::runtime_error("unsupported parenthesis expression");
    }

    llvm::AllocaInst *createVariableSlot(const std::string &name) {
        llvm::IRBuilder<> entry_builder(
            &_main_function->getEntryBlock(),
            _main_function->getEntryBlock().begin());
        return entry_builder.CreateAlloca(_builder.getInt32Ty(), nullptr, name);
    }

    llvm::FunctionCallee getPrintf() {
        if (_printf) {
            return _printf;
        }
        llvm::Type *printf_args[] = {_builder.getPtrTy()};
        auto *printf_type = llvm::FunctionType::get(_builder.getInt32Ty(),
                                                    printf_args, true);
        _printf = _module->getOrInsertFunction("printf", printf_type);
        return _printf;
    }

  private:
    std::unique_ptr<llvm::LLVMContext> _context;
    std::unique_ptr<llvm::Module>      _module;
    llvm::IRBuilder<>                  _builder;
    const grammer::GPackage           &_package;
    llvm::Function                    *_main_function = nullptr;
    llvm::FunctionCallee               _printf;
    std::map<std::string, llvm::AllocaInst *> _variables;
    bool                                    _terminated = false;
};

std::string printModule(llvm::Module &module) {
    std::string              ir_text;
    llvm::raw_string_ostream os(ir_text);
    module.print(os, nullptr);
    return ir_text;
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

bool emitPackageIR(const grammer::GPackage &package,
                   const std::string       &source_path, std::string &ir_text,
                   std::string &error) {
    try {
        ModuleBuilder builder(package, source_path);
        builder.buildMain();
        auto module = builder.releaseModule();
        ir_text     = printModule(*module);
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
}

bool runPackageMain(const grammer::GPackage &package,
                    const std::string      &source_path, int &exit_code,
                    std::string &error) {
    try {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();

        ModuleBuilder builder(package, source_path);
        builder.buildMain();
        auto context = builder.releaseContext();
        auto module  = builder.releaseModule();

        auto jit_or_err = llvm::orc::LLJITBuilder().create();
        if (!jit_or_err) {
            error = llvm::toString(jit_or_err.takeError());
            return false;
        }
        auto jit = std::move(*jit_or_err);

        auto generator_or_err =
            llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
                jit->getDataLayout().getGlobalPrefix());
        if (!generator_or_err) {
            error = llvm::toString(generator_or_err.takeError());
            return false;
        }
        jit->getMainJITDylib().addGenerator(std::move(*generator_or_err));

        auto module_err = jit->addIRModule(llvm::orc::ThreadSafeModule(
            std::move(module), std::move(context)));
        if (module_err) {
            error = llvm::toString(std::move(module_err));
            return false;
        }

        auto symbol_or_err = jit->lookup("main");
        if (!symbol_or_err) {
            error = llvm::toString(symbol_or_err.takeError());
            return false;
        }

        using MainFunc = int (*)();
        auto *entry = symbol_or_err->toPtr<MainFunc>();
        exit_code = entry();
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
}

} // namespace llvm_backend
} // namespace pangu
