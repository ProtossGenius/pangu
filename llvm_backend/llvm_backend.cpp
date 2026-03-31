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

std::string sanitizeSymbolComponent(const std::string &text) {
    std::string sanitized;
    for (char ch : text) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9')) {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }
    return sanitized;
}

std::string functionKey(const std::string &module_id, const std::string &name) {
    return module_id + "::" + name;
}

std::string llvmFunctionName(const std::string &entry_module_id,
                             const std::string &module_id,
                             const std::string &name) {
    if (module_id == entry_module_id && name == "main") {
        return "main";
    }
    return "__pangu_" + sanitizeSymbolComponent(module_id) + "__" + name;
}

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
    ModuleBuilder(const Program &program, const std::string &source_path)
        : _context(new llvm::LLVMContext())
        , _module(new llvm::Module(moduleNameFromPath(source_path), *_context))
        , _builder(*_context)
        , _program(program) {
        _module->setSourceFileName(source_path);
    }

    void buildAllFunctions() {
        declareFunctions();
        defineFunctions();
        if (_declared_functions.count(
                functionKey(_program.entry_module_id, "main")) == 0) {
            throw std::runtime_error("main function not found");
        }
    }

    std::unique_ptr<llvm::LLVMContext> releaseContext() {
        return std::move(_context);
    }

    std::unique_ptr<llvm::Module> releaseModule() { return std::move(_module); }

  private:
    void declareFunctions() {
        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->functions.items()) {
                const auto *function = it.second.get();
                auto       *type     = makeFunctionType(*function);
                _declared_functions[ functionKey(unit.module_id, function->name()) ] =
                    llvm::Function::Create(
                        type, llvm::Function::ExternalLinkage,
                        llvmFunctionName(_program.entry_module_id, unit.module_id,
                                         function->name()),
                        *_module);
            }
        }
    }

    void defineFunctions() {
        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->functions.items()) {
                defineFunction(unit, *it.second);
            }
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
        if (type->name() == "string") {
            return _builder.getPtrTy();
        }
        throw std::runtime_error("unsupported variable type: " + type->name());
    }

    void defineFunction(const PackageUnit &unit, const grammer::GFunction &function) {
        _current_module_id   = unit.module_id;
        _current_imports     = &unit.import_alias_to_module;
        _current_function =
            _declared_functions.at(functionKey(unit.module_id, function.name()));
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
        if (oper == "if") {
            return emitIfStatement(code);
        }
        if (oper == "while") {
            return emitWhileStatement(code);
        }
        if (oper == "for") {
            return emitForStatement(code);
        }
        return emitExpression(code);
    }

    bool isComparisonOperator(const std::string &oper) const {
        return oper == "==" || oper == "!=" || oper == ">" || oper == "<" ||
               oper == ">=" || oper == "<=";
    }

    llvm::Value *emitConditionValue(const pgcodes::GCode *code) {
        auto *value = emitExpression(code);
        if (value->getType()->isIntegerTy(1)) {
            return value;
        }
        if (!value->getType()->isIntegerTy()) {
            throw std::runtime_error("if condition must be integer-compatible");
        }
        return _builder.CreateICmpNE(
            value, llvm::ConstantInt::get(value->getType(), 0), "ifcond");
    }

    llvm::Value *emitIfStatement(const pgcodes::GCode *code) {
        const auto *branch = code->getRight();
        if (branch == nullptr ||
            branch->getValueType() != pgcodes::ValueType::NOT_VALUE ||
            branch->getOper() != ":") {
            throw std::runtime_error("if node misses branch payload");
        }

        auto *condition = emitConditionValue(code->getLeft());
        auto *function  = _current_function;

        auto *then_block =
            llvm::BasicBlock::Create(*_context, "if.then", function);
        auto *else_block = llvm::BasicBlock::Create(*_context, "if.else");
        auto *merge_block = llvm::BasicBlock::Create(*_context, "if.end");
        const bool has_else = branch->getRight() != nullptr;

        _builder.CreateCondBr(condition, then_block,
                              has_else ? else_block : merge_block);

        _builder.SetInsertPoint(then_block);
        _terminated = false;
        emitStatement(branch->getLeft());
        const bool then_terminated =
            _terminated || _builder.GetInsertBlock()->getTerminator() != nullptr;
        if (!then_terminated) {
            _builder.CreateBr(merge_block);
        }

        bool else_terminated = false;
        if (has_else) {
            function->insert(function->end(), else_block);
            _builder.SetInsertPoint(else_block);
            _terminated = false;
            emitStatement(branch->getRight());
            else_terminated = _terminated ||
                              _builder.GetInsertBlock()->getTerminator() != nullptr;
            if (!else_terminated) {
                _builder.CreateBr(merge_block);
            }
        }

        if (!then_terminated || !has_else || !else_terminated) {
            function->insert(function->end(), merge_block);
            _builder.SetInsertPoint(merge_block);
            _terminated = false;
        } else {
            _terminated = true;
        }
        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
    }

    // ── while (cond) { body } ────────────────────────────────────────
    llvm::Value *emitWhileStatement(const pgcodes::GCode *code) {
        auto *function = _current_function;

        auto *cond_block =
            llvm::BasicBlock::Create(*_context, "while.cond", function);
        auto *body_block = llvm::BasicBlock::Create(*_context, "while.body");
        auto *end_block  = llvm::BasicBlock::Create(*_context, "while.end");

        _builder.CreateBr(cond_block);

        // Condition block
        _builder.SetInsertPoint(cond_block);
        auto *condition = emitConditionValue(code->getLeft());
        _builder.CreateCondBr(condition, body_block, end_block);

        // Body block
        function->insert(function->end(), body_block);
        _builder.SetInsertPoint(body_block);
        _terminated = false;
        emitStatement(code->getRight());
        if (!_terminated &&
            _builder.GetInsertBlock()->getTerminator() == nullptr) {
            _builder.CreateBr(cond_block);
        }

        // End block
        function->insert(function->end(), end_block);
        _builder.SetInsertPoint(end_block);
        _terminated = false;
        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
    }

    // ── for (init; cond; step) { body } ──────────────────────────────
    // The header (code->getLeft()) may be wrapped in a `(...)` grouping.
    // Inner content is `;`-separated: init ; cond ; step
    llvm::Value *emitForStatement(const pgcodes::GCode *code) {
        auto *function = _current_function;
        const auto *header = code->getLeft();

        // Unwrap `(` grouping if present
        if (header != nullptr &&
            header->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            header->getOper() == "(") {
            header = header->getRight();
        }

        // Decompose the for-header: init ; cond ; step
        // GCode `;` is left-associative: `;`(`;`(init, cond), step)
        const pgcodes::GCode *init_code = nullptr;
        const pgcodes::GCode *cond_code = nullptr;
        const pgcodes::GCode *step_code = nullptr;

        if (header != nullptr &&
            header->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            header->getOper() == ";") {
            step_code = header->getRight();
            const auto *init_cond = header->getLeft();
            if (init_cond != nullptr &&
                init_cond->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                init_cond->getOper() == ";") {
                init_code = init_cond->getLeft();
                cond_code = init_cond->getRight();
            } else {
                init_code = init_cond;
            }
        }

        // Emit init
        if (init_code != nullptr) {
            emitExpression(init_code);
        }

        auto *cond_block =
            llvm::BasicBlock::Create(*_context, "for.cond", function);
        auto *body_block = llvm::BasicBlock::Create(*_context, "for.body");
        auto *step_block = llvm::BasicBlock::Create(*_context, "for.step");
        auto *end_block  = llvm::BasicBlock::Create(*_context, "for.end");

        _builder.CreateBr(cond_block);

        // Condition
        _builder.SetInsertPoint(cond_block);
        if (cond_code != nullptr) {
            auto *condition = emitConditionValue(cond_code);
            _builder.CreateCondBr(condition, body_block, end_block);
        } else {
            _builder.CreateBr(body_block); // infinite loop
        }

        // Body
        function->insert(function->end(), body_block);
        _builder.SetInsertPoint(body_block);
        _terminated = false;
        emitStatement(code->getRight());
        if (!_terminated &&
            _builder.GetInsertBlock()->getTerminator() == nullptr) {
            _builder.CreateBr(step_block);
        }

        // Step
        function->insert(function->end(), step_block);
        _builder.SetInsertPoint(step_block);
        if (step_code != nullptr) {
            emitExpression(step_code);
        }
        _builder.CreateBr(cond_block);

        // End
        function->insert(function->end(), end_block);
        _builder.SetInsertPoint(end_block);
        _terminated = false;
        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
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

    bool extractQualifiedCall(const pgcodes::GCode *code, std::string &module_alias,
                              std::string &callee,
                              const pgcodes::GCode *&args_code) {
        if (code == nullptr || code->getValueType() != pgcodes::ValueType::NOT_VALUE ||
            code->getOper() != ".") {
            return false;
        }
        const auto *left  = code->getLeft();
        const auto *right = code->getRight();
        if (left == nullptr || right == nullptr ||
            left->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            return false;
        }
        module_alias = left->getValue();

        if (right->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            right->getOper() == "(" && right->getLeft() != nullptr &&
            right->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            callee    = right->getLeft()->getValue();
            args_code = right->getRight();
            return true;
        }

        if (right->getValueType() == pgcodes::ValueType::IDENTIFIER &&
            isSuffixCallNode(right->getRight())) {
            callee    = right->getValue();
            args_code = right->getRight()->getRight();
            return true;
        }
        return false;
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
                return emitCall(resolveCurrentFunction(callee), callee, args_code);
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
        if (oper == "%") {
            return _builder.CreateSRem(emitExpression(code->getLeft()),
                                       emitExpression(code->getRight()),
                                       "modtmp");
        }
        if (oper == "&&") {
            return emitLogicalAnd(code);
        }
        if (oper == "||") {
            return emitLogicalOr(code);
        }
        if (oper == "!") {
            auto *val = emitExpression(code->getRight());
            if (val->getType()->isIntegerTy(1)) {
                return _builder.CreateNot(val, "nottmp");
            }
            return _builder.CreateICmpEQ(
                val, llvm::ConstantInt::get(val->getType(), 0), "nottmp");
        }
        if (isComparisonOperator(oper)) {
            return emitComparison(code, oper);
        }
        if (oper == "(") {
            return emitParenOrCall(code);
        }
        if (oper == ".") {
            std::string          module_alias;
            std::string          callee;
            const pgcodes::GCode *args_code = nullptr;
            if (extractQualifiedCall(code, module_alias, callee, args_code)) {
                return emitQualifiedCall(module_alias, callee, args_code);
            }
        }
        if (oper == "{") {
            emitStatement(code);
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (oper == "++" || oper == "--") {
            return emitIncDec(code, oper == "++");
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
        if (isComparisonOperator(oper)) {
            return emitComparison(code, oper);
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
            auto *alloca_type = it->second->getAllocatedType();
            return _builder.CreateLoad(alloca_type, it->second,
                                       code->getValue());
        }
        case pgcodes::ValueType::STRING: {
            std::string raw = code->getValue();
            // Strip surrounding quotes if present
            if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
                raw = raw.substr(1, raw.size() - 2);
            }
            // Process escape sequences
            std::string processed;
            for (size_t i = 0; i < raw.size(); ++i) {
                if (raw[i] == '\\' && i + 1 < raw.size()) {
                    switch (raw[i + 1]) {
                    case 'n': processed += '\n'; ++i; break;
                    case 't': processed += '\t'; ++i; break;
                    case '\\': processed += '\\'; ++i; break;
                    case '"': processed += '"'; ++i; break;
                    default: processed += raw[i]; break;
                    }
                } else {
                    processed += raw[i];
                }
            }
            return _builder.CreateGlobalStringPtr(processed, "str");
        }
        case pgcodes::ValueType::NOT_VALUE: break;
        }
        throw std::runtime_error("unexpected empty value");
    }

    llvm::Value *emitComparison(const pgcodes::GCode *code,
                                const std::string   &oper) {
        auto *left  = emitExpression(code->getLeft());
        auto *right = emitExpression(code->getRight());
        llvm::Value *cmp = nullptr;

        if (oper == "==") {
            cmp = _builder.CreateICmpEQ(left, right, "cmptmp");
        } else if (oper == "!=") {
            cmp = _builder.CreateICmpNE(left, right, "cmptmp");
        } else if (oper == ">") {
            cmp = _builder.CreateICmpSGT(left, right, "cmptmp");
        } else if (oper == "<") {
            cmp = _builder.CreateICmpSLT(left, right, "cmptmp");
        } else if (oper == ">=") {
            cmp = _builder.CreateICmpSGE(left, right, "cmptmp");
        } else if (oper == "<=") {
            cmp = _builder.CreateICmpSLE(left, right, "cmptmp");
        } else {
            throw std::runtime_error("unsupported comparison operator: " + oper);
        }

        return _builder.CreateZExt(cmp, _builder.getInt32Ty(), "booltmp");
    }

    // Short-circuit &&: if LHS is false, skip RHS
    llvm::Value *emitLogicalAnd(const pgcodes::GCode *code) {
        auto *function = _current_function;
        auto *rhs_block   = llvm::BasicBlock::Create(*_context, "and.rhs");
        auto *merge_block = llvm::BasicBlock::Create(*_context, "and.end");

        auto *lhs = emitConditionValue(code->getLeft());
        auto *lhs_bb = _builder.GetInsertBlock();
        _builder.CreateCondBr(lhs, rhs_block, merge_block);

        function->insert(function->end(), rhs_block);
        _builder.SetInsertPoint(rhs_block);
        auto *rhs = emitConditionValue(code->getRight());
        auto *rhs_bb = _builder.GetInsertBlock();
        _builder.CreateBr(merge_block);

        function->insert(function->end(), merge_block);
        _builder.SetInsertPoint(merge_block);
        auto *phi = _builder.CreatePHI(_builder.getInt1Ty(), 2, "and.result");
        phi->addIncoming(llvm::ConstantInt::getFalse(*_context), lhs_bb);
        phi->addIncoming(rhs, rhs_bb);
        return _builder.CreateZExt(phi, _builder.getInt32Ty(), "andtmp");
    }

    // Short-circuit ||: if LHS is true, skip RHS
    llvm::Value *emitLogicalOr(const pgcodes::GCode *code) {
        auto *function = _current_function;
        auto *rhs_block   = llvm::BasicBlock::Create(*_context, "or.rhs");
        auto *merge_block = llvm::BasicBlock::Create(*_context, "or.end");

        auto *lhs = emitConditionValue(code->getLeft());
        auto *lhs_bb = _builder.GetInsertBlock();
        _builder.CreateCondBr(lhs, merge_block, rhs_block);

        function->insert(function->end(), rhs_block);
        _builder.SetInsertPoint(rhs_block);
        auto *rhs = emitConditionValue(code->getRight());
        auto *rhs_bb = _builder.GetInsertBlock();
        _builder.CreateBr(merge_block);

        function->insert(function->end(), merge_block);
        _builder.SetInsertPoint(merge_block);
        auto *phi = _builder.CreatePHI(_builder.getInt1Ty(), 2, "or.result");
        phi->addIncoming(llvm::ConstantInt::getTrue(*_context), lhs_bb);
        phi->addIncoming(rhs, rhs_bb);
        return _builder.CreateZExt(phi, _builder.getInt32Ty(), "ortmp");
    }

    llvm::Value *emitAssignment(const pgcodes::GCode *code, bool define_new) {
        const auto *left = code->getLeft();
        if (left == nullptr ||
            left->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("assignment left value must be identifier");
        }

        const std::string name = left->getValue();
        auto *value = emitExpression(code->getRight());

        llvm::AllocaInst  *slot = nullptr;
        auto               it   = _variables.find(name);
        if (it == _variables.end()) {
            if (!define_new) {
                throw std::runtime_error("assign to undefined identifier: " +
                                         name);
            }
            slot             = createVariableSlot(name, value->getType());
            _variables[name] = slot;
        } else {
            slot = it->second;
        }

        _builder.CreateStore(value, slot);
        return value;
    }

    llvm::Value *emitIncDec(const pgcodes::GCode *code, bool is_inc) {
        const auto *operand = code->getRight();
        if (operand == nullptr ||
            operand->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("++/-- operand must be an identifier");
        }
        const std::string &name = operand->getValue();
        auto it = _variables.find(name);
        if (it == _variables.end()) {
            throw std::runtime_error("++/-- on undefined variable: " + name);
        }
        auto *current = _builder.CreateLoad(_builder.getInt32Ty(), it->second);
        auto *one     = llvm::ConstantInt::get(_builder.getInt32Ty(), 1);
        auto *result  = is_inc ? _builder.CreateAdd(current, one, "inc")
                               : _builder.CreateSub(current, one, "dec");
        _builder.CreateStore(result, it->second);
        return result;
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

    llvm::Function *resolveCurrentFunction(const std::string &callee) {
        const std::string key = functionKey(_current_module_id, callee);
        auto              it  = _declared_functions.find(key);
        if (it == _declared_functions.end()) {
            return nullptr;
        }
        return it->second;
    }

    llvm::Function *resolveImportedFunction(const std::string &module_alias,
                                            const std::string &callee) {
        if (_current_imports == nullptr || _current_imports->count(module_alias) == 0) {
            return nullptr;
        }
        const std::string key =
            functionKey(_current_imports->at(module_alias), callee);
        auto it = _declared_functions.find(key);
        if (it == _declared_functions.end()) {
            return nullptr;
        }
        return it->second;
    }

    llvm::Value *emitCall(llvm::Function *callee_function,
                          const std::string &callee,
                          const pgcodes::GCode *args_code) {
        if (callee == "println") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1) {
                throw std::runtime_error(
                    "println currently supports exactly one argument");
            }
            auto *arg = args.front();
            if (arg->getType()->isPointerTy()) {
                auto *fmt = _builder.CreateGlobalStringPtr("%s\n");
                _builder.CreateCall(getPrintf(), {fmt, arg});
            } else {
                auto *fmt = _builder.CreateGlobalStringPtr("%d\n");
                _builder.CreateCall(getPrintf(), {fmt, arg});
            }
            return arg;
        }
        if (callee == "print") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1) {
                throw std::runtime_error(
                    "print currently supports exactly one argument");
            }
            auto *arg = args.front();
            if (arg->getType()->isPointerTy()) {
                auto *fmt = _builder.CreateGlobalStringPtr("%s");
                _builder.CreateCall(getPrintf(), {fmt, arg});
            } else {
                auto *fmt = _builder.CreateGlobalStringPtr("%d");
                _builder.CreateCall(getPrintf(), {fmt, arg});
            }
            return arg;
        }
        if (callee == "exit") {
            auto args = emitCallArgs(args_code);
            auto *code = args.empty()
                             ? llvm::ConstantInt::get(_builder.getInt32Ty(), 0)
                             : args.front();
            _builder.CreateCall(getExit(), {code});
            _builder.CreateUnreachable();
            _terminated = true;
            return code;
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
        if (callee_function == nullptr) {
            throw std::runtime_error("unsupported function call: " + callee);
        }
        auto args = emitCallArgs(args_code);
        return _builder.CreateCall(callee_function, args,
                                    callee + ".call");
    }

    llvm::Value *emitQualifiedCall(const std::string &module_alias,
                                   const std::string &callee,
                                   const pgcodes::GCode *args_code) {
        auto *callee_function = resolveImportedFunction(module_alias, callee);
        if (callee_function == nullptr) {
            throw std::runtime_error("unsupported imported function call: " +
                                     module_alias + "." + callee);
        }
        return emitCall(callee_function, module_alias + "." + callee, args_code);
    }

    llvm::Value *emitParenOrCall(const pgcodes::GCode *code) {
        const auto *left = code->getLeft();
        if (left == nullptr) {
            throw std::runtime_error("call expression misses callee");
        }
        if (left->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            return emitCall(resolveCurrentFunction(left->getValue()),
                            left->getValue(), code->getRight());
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

    llvm::FunctionCallee getExit() {
        if (_exit) {
            return _exit;
        }
        llvm::Type *exit_args[] = {_builder.getInt32Ty()};
        auto *exit_type = llvm::FunctionType::get(_builder.getVoidTy(),
                                                  exit_args, false);
        _exit = _module->getOrInsertFunction("exit", exit_type);
        return _exit;
    }

  private:
    std::unique_ptr<llvm::LLVMContext> _context;
    std::unique_ptr<llvm::Module>      _module;
    llvm::IRBuilder<>                  _builder;
    const Program                     &_program;
    llvm::Function                    *_current_function = nullptr;
    llvm::FunctionCallee               _printf;
    llvm::FunctionCallee               _exit;
    std::map<std::string, llvm::Function *>    _declared_functions;
    std::map<std::string, llvm::AllocaInst *>  _variables;
    std::string                                _current_module_id;
    const std::map<std::string, std::string>  *_current_imports = nullptr;
    bool                                      _terminated = false;
};

std::string printModule(llvm::Module &module) {
    std::string              ir_text;
    llvm::raw_string_ostream os(ir_text);
    module.print(os, nullptr);
    return ir_text;
}

bool buildProgramModule(const Program &program,
                        const std::string       &source_path,
                        std::unique_ptr<llvm::LLVMContext> &context,
                        std::unique_ptr<llvm::Module>      &module,
                        std::string                        &error) {
    try {
        ModuleBuilder builder(program, source_path);
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

bool emitProgramIR(const Program &program,
                   const std::string       &source_path, std::string &ir_text,
                   std::string &error) {
    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module>      module;
    if (!buildProgramModule(program, source_path, context, module, error)) {
        return false;
    }
    ir_text = printModule(*module);
    return true;
}

bool runProgramMain(const Program &program,
                    const std::string      &source_path, int &exit_code,
                    std::string &error) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::unique_ptr<llvm::LLVMContext> context;
    std::unique_ptr<llvm::Module>      module;
    if (!buildProgramModule(program, source_path, context, module, error)) {
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

bool compileProgramToExecutable(const Program            &program,
                                const std::string      &source_path,
                                const std::string      &output_path,
                                std::string            &error) {
    std::string ir_text;
    if (!emitProgramIR(program, source_path, ir_text, error)) {
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
