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
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

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

std::string quotePath(const std::string &path) {
    std::string quoted = "\"";
    for (char ch : path) {
        if (ch == '"' || ch == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(ch);
    }
    quoted.push_back('"');
    return quoted;
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

    void buildAllFunctions() {
        declareFunctions();
        defineFunctions();
        if (_declared_functions.count("main") == 0) {
            throw std::runtime_error("main function not found");
        }
    }

    std::unique_ptr<llvm::LLVMContext> releaseContext() {
        return std::move(_context);
    }

    std::unique_ptr<llvm::Module> releaseModule() { return std::move(_module); }

  private:
    void declareFunctions() {
        for (const auto &it : _package.functions.items()) {
            const auto *function = it.second.get();
            auto       *type     = makeFunctionType(*function);
            _declared_functions[ function->name() ] = llvm::Function::Create(
                type, llvm::Function::ExternalLinkage, function->name(),
                *_module);
        }
    }

    void defineFunctions() {
        for (const auto &it : _package.functions.items()) {
            defineFunction(*it.second);
        }
    }

    llvm::FunctionType *makeFunctionType(const grammer::GFunction &function) {
        if (function.result.size() > 1) {
            throw std::runtime_error("multiple return values are not supported");
        }

        std::vector<llvm::Type *> params;
        for (const auto &name : function.params.orderedNames()) {
            const auto *var = function.params.getVariable(name);
            params.push_back(getLLVMType(*var));
        }

        llvm::Type *return_type = _builder.getInt32Ty();
        if (function.name() == "main") {
            return_type = _builder.getInt32Ty();
        } else if (function.result.size() == 1) {
            return_type = getLLVMType(*function.result.getVariable(
                function.result.orderedNames().front()));
        }

        return llvm::FunctionType::get(return_type, params, false);
    }

    llvm::Type *getLLVMType(const grammer::GVarDef &var) {
        const auto *type = var.getType();
        if (type->name() == "int") {
            return _builder.getInt32Ty();
        }
        throw std::runtime_error("unsupported variable type: " + type->name());
    }

    void defineFunction(const grammer::GFunction &function) {
        _current_function = _declared_functions.at(function.name());
        _variables.clear();
        _terminated = false;

        auto *entry =
            llvm::BasicBlock::Create(*_context, "entry", _current_function);
        _builder.SetInsertPoint(entry);

        bindParameters(function);
        emitStatement(function.code.get());
        if (!_terminated) {
            _builder.CreateRet(llvm::ConstantInt::get(_builder.getInt32Ty(), 0));
        }
    }

    void bindParameters(const grammer::GFunction &function) {
        size_t index = 0;
        for (auto &arg : _current_function->args()) {
            const std::string &name = function.params.orderedNames().at(index++);
            arg.setName(name);
            auto *slot = createVariableSlot(name, arg.getType());
            _builder.CreateStore(&arg, slot);
            _variables[name] = slot;
        }
    }

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

    bool isSuffixCallNode(const pgcodes::GCode *code) {
        return code != nullptr &&
               code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
               code->getOper() == "(" && code->getLeft() == nullptr;
    }

    bool extractSuffixCall(const pgcodes::GCode *code, std::string &callee,
                           const pgcodes::GCode *&args_code) {
        if (code == nullptr ||
            code->getValueType() != pgcodes::ValueType::IDENTIFIER ||
            !isSuffixCallNode(code->getRight())) {
            return false;
        }
        callee    = code->getValue();
        args_code = code->getRight()->getRight();
        return true;
    }

    bool containsReturnPrefix(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return false;
        }
        std::string          callee;
        const pgcodes::GCode *args_code = nullptr;
        if (extractSuffixCall(code, callee, args_code) && callee == "return") {
            return true;
        }
        return containsReturnPrefix(code->getLeft()) ||
               containsReturnPrefix(code->getRight());
    }

    llvm::Value *emitReturnExpressionValue(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        std::string          callee;
        const pgcodes::GCode *args_code = nullptr;
        if (extractSuffixCall(code, callee, args_code) && callee == "return") {
            auto args = emitCallArgs(args_code);
            return args.empty() ? llvm::ConstantInt::get(_builder.getInt32Ty(), 0)
                                : args.front();
        }
        return emitExpression(code);
    }

    llvm::Value *emitExpression(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (containsReturnPrefix(code)) {
            auto *value = emitReturnExpressionTree(code);
            _builder.CreateRet(value);
            _terminated = true;
            return value;
        }
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            std::string          callee;
            const pgcodes::GCode *args_code = nullptr;
            if (extractSuffixCall(code, callee, args_code)) {
                return emitCall(callee, args_code);
            }
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

    llvm::Value *emitReturnExpressionTree(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            return emitReturnExpressionValue(code);
        }

        const std::string oper = code->getOper();
        if (oper == "+") {
            return _builder.CreateAdd(emitReturnExpressionTree(code->getLeft()),
                                      emitReturnExpressionTree(code->getRight()),
                                      "retadd");
        }
        if (oper == "-") {
            return _builder.CreateSub(emitReturnExpressionTree(code->getLeft()),
                                      emitReturnExpressionTree(code->getRight()),
                                      "retsub");
        }
        if (oper == "*") {
            return _builder.CreateMul(emitReturnExpressionTree(code->getLeft()),
                                      emitReturnExpressionTree(code->getRight()),
                                      "retmul");
        }
        if (oper == "/") {
            return _builder.CreateSDiv(
                emitReturnExpressionTree(code->getLeft()),
                emitReturnExpressionTree(code->getRight()), "retdiv");
        }
        if (oper == "(") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                code->getLeft()->getOper() == ")") {
                return emitReturnExpressionTree(code->getRight());
            }
            return emitParenOrCall(code);
        }
        throw std::runtime_error("unsupported return expression operator: " +
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

        const std::string name = left->getValue();
        llvm::AllocaInst  *slot = nullptr;
        auto               it   = _variables.find(name);
        if (it == _variables.end()) {
            if (!define_new) {
                throw std::runtime_error("assign to undefined identifier: " +
                                         name);
            }
            slot             = createVariableSlot(name, _builder.getInt32Ty());
            _variables[name] = slot;
        } else {
            slot = it->second;
        }

        auto *value = emitExpression(code->getRight());
        _builder.CreateStore(value, slot);
        return value;
    }

    const pgcodes::GCode *stripGrouping(const pgcodes::GCode *code) {
        const auto *current = code;
        while (current != nullptr &&
               current->getValueType() == pgcodes::ValueType::NOT_VALUE &&
               current->getOper() == "(" && current->getLeft() != nullptr &&
               current->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
               current->getLeft()->getOper() == ")") {
            current = current->getRight();
        }
        return current;
    }

    void collectArgNodes(const pgcodes::GCode              *code,
                         std::vector<const pgcodes::GCode *> &out) {
        const auto *current = stripGrouping(code);
        if (current == nullptr) {
            return;
        }
        if (current->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            current->getOper() == ",") {
            collectArgNodes(current->getLeft(), out);
            collectArgNodes(current->getRight(), out);
            return;
        }
        out.push_back(current);
    }

    std::vector<llvm::Value *> emitCallArgs(const pgcodes::GCode *code) {
        std::vector<const pgcodes::GCode *> arg_nodes;
        collectArgNodes(code, arg_nodes);

        std::vector<llvm::Value *> args;
        for (const auto *arg_node : arg_nodes) {
            args.push_back(emitExpression(arg_node));
        }
        return args;
    }

    llvm::Value *emitCall(const std::string &callee,
                          const pgcodes::GCode *args_code) {
        if (callee == "println") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1) {
                throw std::runtime_error(
                    "println currently supports exactly one argument");
            }
            auto *fmt = _builder.CreateGlobalStringPtr("%d\n");
            _builder.CreateCall(getPrintf(), {fmt, args.front()});
            return args.front();
        }
        if (callee == "return") {
            auto args = emitCallArgs(args_code);
            auto *value =
                args.empty() ? llvm::ConstantInt::get(_builder.getInt32Ty(), 0)
                             : args.front();
            _builder.CreateRet(value);
            _terminated = true;
            return value;
        }
        if (_declared_functions.count(callee) == 0) {
            throw std::runtime_error("unsupported function call: " + callee);
        }
        auto args = emitCallArgs(args_code);
        return _builder.CreateCall(_declared_functions.at(callee), args,
                                   callee + ".call");
    }

    llvm::Value *emitParenOrCall(const pgcodes::GCode *code) {
        const auto *left = code->getLeft();
        if (left == nullptr) {
            throw std::runtime_error("call expression misses callee");
        }
        if (left->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            return emitCall(left->getValue(), code->getRight());
        }
        if (left->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            left->getOper() == ")") {
            return emitExpression(code->getRight());
        }
        throw std::runtime_error("unsupported parenthesis expression");
    }

    llvm::AllocaInst *createVariableSlot(const std::string &name,
                                         llvm::Type        *type) {
        llvm::IRBuilder<> entry_builder(&_current_function->getEntryBlock(),
                                        _current_function->getEntryBlock().begin());
        return entry_builder.CreateAlloca(type, nullptr, name);
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
    llvm::Function                    *_current_function = nullptr;
    llvm::FunctionCallee               _printf;
    std::map<std::string, llvm::Function *>    _declared_functions;
    std::map<std::string, llvm::AllocaInst *>  _variables;
    bool                                      _terminated = false;
};

std::string printModule(llvm::Module &module) {
    std::string              ir_text;
    llvm::raw_string_ostream os(ir_text);
    module.print(os, nullptr);
    return ir_text;
}

bool buildPackageModule(const grammer::GPackage &package,
                        const std::string       &source_path,
                        std::unique_ptr<llvm::LLVMContext> &context,
                        std::unique_ptr<llvm::Module>      &module,
                        std::string                        &error) {
    try {
        ModuleBuilder builder(package, source_path);
        builder.buildAllFunctions();
        context = builder.releaseContext();
        module  = builder.releaseModule();
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
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

    return printModule(module);
}

bool emitPackageIR(const grammer::GPackage &package,
                   const std::string       &source_path, std::string &ir_text,
                   std::string &error) {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module>      module;
    if (!buildPackageModule(package, source_path, context, module, error)) {
        return false;
    }
    ir_text = printModule(*module);
    return true;
}

bool runPackageMain(const grammer::GPackage &package,
                    const std::string      &source_path, int &exit_code,
                    std::string &error) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module>      module;
    if (!buildPackageModule(package, source_path, context, module, error)) {
        return false;
    }

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

    auto module_err = jit->addIRModule(
        llvm::orc::ThreadSafeModule(std::move(module), std::move(context)));
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
    exit_code   = entry();
    return true;
}

bool compilePackageToExecutable(const grammer::GPackage &package,
                                const std::string      &source_path,
                                const std::string      &output_path,
                                std::string            &error) {
    std::string ir_text;
    if (!emitPackageIR(package, source_path, ir_text, error)) {
        return false;
    }

    const std::string ir_path = output_path + ".ll";
    std::ofstream     ir_file(ir_path);
    if (!ir_file.good()) {
        error = "can not write ir file: " + ir_path;
        return false;
    }
    ir_file << ir_text;
    ir_file.close();

    const std::string command = "clang -Wno-override-module -x ir " +
                                quotePath(ir_path) + " -o " +
                                quotePath(output_path);
    const int rc = std::system(command.c_str());
    if (rc != 0) {
        error = "clang failed when compiling llvm ir";
        return false;
    }
    return true;
}

} // namespace llvm_backend
} // namespace pangu
