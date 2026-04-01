#include "sema/sema.h"

#include "grammer/datas.h"
#include "pgcodes/datas.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace pangu {
namespace sema {
namespace {

struct FunctionSig {
    std::string name;
    size_t      param_count = 0;
};

using FunctionTable = std::map<std::string, FunctionSig>;
using ModuleTable   = std::map<std::string, FunctionTable>;

const std::set<std::string> BUILTIN_FUNCTIONS = {
    "println", "print", "exit",
    "str_concat", "str_len", "str_eq", "str_substr",
    "int_to_str", "str_to_int",
    "read_file", "write_file",
    "str_char_at", "char_to_str",
    "make_array", "array_get", "array_set",
    "make_str_array", "str_array_get", "str_array_set",
    "args", "args_count",
};

bool isBuiltin(const std::string &name) {
    return BUILTIN_FUNCTIONS.count(name) != 0;
}

size_t builtinParamCount(const std::string &name) {
    if (name == "println" || name == "print" || name == "exit") return 1;
    if (name == "str_len" || name == "int_to_str" || name == "str_to_int")
        return 1;
    if (name == "read_file" || name == "char_to_str") return 1;
    if (name == "make_array" || name == "make_str_array") return 1;
    if (name == "args") return 1;
    if (name == "args_count") return 0;
    if (name == "str_concat" || name == "str_eq") return 2;
    if (name == "write_file" || name == "str_char_at") return 2;
    if (name == "array_get" || name == "str_array_get") return 2;
    if (name == "str_substr") return 3;
    if (name == "array_set" || name == "str_array_set") return 3;
    return 0;
}

// Collect all `,`-separated argument nodes from a call expression.
void collectArgNodes(const pgcodes::GCode              *code,
                     std::vector<const pgcodes::GCode *> &out) {
    if (code == nullptr) return;
    // Strip grouping parentheses.
    const auto *current = code;
    while (current != nullptr &&
           current->getValueType() == pgcodes::ValueType::NOT_VALUE &&
           current->getOper() == "(" && current->getLeft() != nullptr &&
           current->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
           current->getLeft()->getOper() == ")") {
        current = current->getRight();
    }
    if (current == nullptr) return;
    if (current->getValueType() == pgcodes::ValueType::NOT_VALUE &&
        current->getOper() == ",") {
        collectArgNodes(current->getLeft(), out);
        collectArgNodes(current->getRight(), out);
        return;
    }
    out.push_back(current);
}

size_t countArgs(const pgcodes::GCode *args_code) {
    if (args_code == nullptr) return 0;
    // Empty arg: identifier with empty value (from `func()`)
    if (args_code->getValueType() == pgcodes::ValueType::IDENTIFIER &&
        args_code->getValue().empty()) {
        return 0;
    }
    // Empty arg: NOT_VALUE with empty operator (from `func()` pattern 2)
    if (args_code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
        args_code->getOper().empty()) {
        return 0;
    }
    std::vector<const pgcodes::GCode *> nodes;
    collectArgNodes(args_code, nodes);
    return nodes.size();
}

bool isSuffixCallNode(const pgcodes::GCode *code) {
    return code != nullptr &&
           code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
           code->getOper() == "(" && code->getLeft() == nullptr;
}

class ProgramChecker {
  public:
    explicit ProgramChecker(const llvm_backend::Program &program)
        : _program(program) {}

    CheckResult run() {
        buildSymbolTables();
        checkAllFunctions();
        return _result;
    }

  private:
    void buildSymbolTables() {
        for (const auto &unit : _program.packages) {
            FunctionTable table;
            for (const auto &it : unit.package->functions.items()) {
                const auto *func = it.second.get();
                const std::string &name = func->name();
                if (table.count(name) != 0) {
                    _result.addError(unit.source_path +
                                     ": error: duplicate function definition '" +
                                     name + "'");
                    continue;
                }
                table[name] = {name, func->params.size()};
            }
            _module_functions[unit.module_id] = std::move(table);

            // Collect struct type names
            for (const auto &it : unit.package->structs.items()) {
                _struct_names.insert(it.first);
            }
        }
    }

    void checkAllFunctions() {
        for (const auto &unit : _program.packages) {
            _current_module_id = unit.module_id;
            _current_imports   = &unit.import_alias_to_module;
            for (const auto &it : unit.package->functions.items()) {
                _current_func_name = it.second->name();
                _defined_vars.clear();
                // Register parameters as defined variables.
                for (const auto &pname : it.second->params.orderedNames()) {
                    _defined_vars.insert(pname);
                }
                checkStatement(it.second->code.get());
            }
        }
    }

    // ── Statement-level walk ─────────────────────────────────────────

    void checkStatement(const pgcodes::GCode *code) {
        if (code == nullptr) return;

        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            checkExpression(code);
            return;
        }

        const std::string oper = code->getOper();
        if (oper == "{") {
            checkStatement(code->getRight());
            return;
        }
        if (oper == ";") {
            checkStatement(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        if (oper == "if") {
            checkExpression(code->getLeft());
            if (code->getRight() != nullptr) {
                // `:` node: left=then, right=else
                checkStatement(code->getRight()->getLeft());
                checkStatement(code->getRight()->getRight());
            }
            return;
        }
        if (oper == "while") {
            checkExpression(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        if (oper == "for") {
            // for (init; cond; step) body
            // code->getLeft() is the header, code->getRight() is body
            checkStatement(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        if (oper == "switch") {
            checkExpression(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        checkExpression(code);
    }

    // ── Expression-level walk ────────────────────────────────────────

    void checkExpression(const pgcodes::GCode *code) {
        if (code == nullptr) return;

        if (code->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string &name = code->getValue();
            // Check for suffix call: `name(args...)`
            if (isSuffixCallNode(code->getRight())) {
                checkLocalCall(code, name, code->getRight()->getRight());
            }
            // Struct literal (suffix form): StructName{field: val, ...}
            else if (code->getRight() != nullptr &&
                     code->getRight()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                     code->getRight()->getOper() == "{" &&
                     _struct_names.count(name) != 0) {
                checkStructLiteral(code->getRight()->getRight());
            }
            else if (code->getRight() != nullptr) {
                // Not a call, recurse on right (might be a chain).
                checkIdentifierRef(code, name);
                checkExpression(code->getRight());
            } else {
                checkIdentifierRef(code, name);
            }
            return;
        }

        if (code->getValueType() == pgcodes::ValueType::NUMBER ||
            code->getValueType() == pgcodes::ValueType::STRING) {
            checkExpression(code->getRight());
            return;
        }

        // Operator nodes.
        const std::string oper = code->getOper();

        if (oper == ":=") {
            // Define-assignment: left must be identifier.
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                _defined_vars.insert(code->getLeft()->getValue());
            }
            checkExpression(code->getRight());
            return;
        }
        if (oper == "=") {
            // Assignment: left must be already defined.
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                checkIdentifierRef(code->getLeft(), code->getLeft()->getValue());
            }
            checkExpression(code->getRight());
            return;
        }
        if (oper == "(") {
            // Parenthesized expr or call.
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                checkLocalCall(code->getLeft(), code->getLeft()->getValue(),
                               code->getRight());
            } else {
                checkExpression(code->getLeft());
                checkExpression(code->getRight());
            }
            return;
        }
        if (oper == "{") {
            // Struct literal: { left=StructName, right=field:val,... }
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                _struct_names.count(code->getLeft()->getValue()) != 0) {
                checkStructLiteral(code->getRight());
            } else {
                checkExpression(code->getLeft());
                checkExpression(code->getRight());
            }
            return;
        }
        if (oper == ".") {
            checkDotExpression(code);
            return;
        }
        // Generic binary/unary operator: recurse both sides.
        checkExpression(code->getLeft());
        checkExpression(code->getRight());
    }

    // ── Specific checks ──────────────────────────────────────────────

    void checkCallArgs(const pgcodes::GCode *args_code) {
        if (args_code == nullptr) return;
        std::vector<const pgcodes::GCode *> nodes;
        collectArgNodes(args_code, nodes);
        for (const auto *node : nodes) {
            checkExpression(node);
        }
    }

    void checkLocalCall(const pgcodes::GCode *node,
                        const std::string &name,
                        const pgcodes::GCode *args_code) {
        // `return` is handled as a special prefix, not a real call.
        if (name == "return") {
            checkExpression(args_code);
            return;
        }

        const auto &loc = node->location();
        size_t name_width = name.size();
        size_t actual_args = countArgs(args_code);

        if (isBuiltin(name)) {
            size_t expected = builtinParamCount(name);
            if (actual_args != expected) {
                emitError(loc, "builtin '" + name + "' expects " +
                          std::to_string(expected) + " argument(s), got " +
                          std::to_string(actual_args), name_width);
            }
            checkCallArgs(args_code);
            return;
        }

        // Look up in current module.
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it == _module_functions.end()) return;
        auto func_it = mod_it->second.find(name);
        if (func_it == mod_it->second.end()) {
            emitError(loc, "undefined function '" + name + "'", name_width);
            checkCallArgs(args_code);
            return;
        }
        if (actual_args != func_it->second.param_count) {
            emitError(loc, "function '" + name + "' expects " +
                      std::to_string(func_it->second.param_count) +
                      " argument(s), got " + std::to_string(actual_args),
                      name_width);
        }
        checkCallArgs(args_code);
    }

    void checkDotExpression(const pgcodes::GCode *code) {
        const auto *left  = code->getLeft();
        const auto *right = code->getRight();
        if (left == nullptr || right == nullptr) return;

        // Field access: expr.field (right is plain identifier, no call suffix)
        if (right->getValueType() == pgcodes::ValueType::IDENTIFIER &&
            !isSuffixCallNode(right->getRight())) {
            // Validate the left side expression; field name itself is not a variable
            checkExpression(left);
            return;
        }

        // Import-qualified call: alias.func(...)
        if (left->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            checkExpression(left);
            checkExpression(right);
            return;
        }

        const std::string &alias = left->getValue();

        // Extract callee name from the right side.
        std::string callee;
        const pgcodes::GCode *args_code = nullptr;

        // Pattern 1: right is `(` node with left=identifier
        if (right->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            right->getOper() == "(" && right->getLeft() != nullptr &&
            right->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            callee    = right->getLeft()->getValue();
            args_code = right->getRight();
        }
        // Pattern 2: right is identifier with suffix `(`
        else if (right->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                 isSuffixCallNode(right->getRight())) {
            callee    = right->getValue();
            args_code = right->getRight()->getRight();
        }
        else {
            // Not a qualified call pattern, just recurse.
            checkExpression(right);
            return;
        }

        checkImportedOrMethodCall(left, alias, callee, args_code);
    }

    bool isStructMethodCall(const std::string &type_name,
                            const std::string &method_name,
                            size_t actual_args) {
        // Check if StructName.method_name exists in current module
        std::string mangled = type_name + "." + method_name;
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it == _module_functions.end()) return false;
        auto func_it = mod_it->second.find(mangled);
        return func_it != mod_it->second.end();
    }

    void checkImportedOrMethodCall(const pgcodes::GCode *alias_node,
                           const std::string &alias,
                           const std::string &callee,
                           const pgcodes::GCode *args_code) {
        // First check if this is a struct method call: StructName.method(args)
        std::string mangled = alias + "." + callee;
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it != _module_functions.end()) {
            auto func_it = mod_it->second.find(mangled);
            if (func_it != mod_it->second.end()) {
                // It's a struct method call
                size_t actual_args = countArgs(args_code);
                if (actual_args != func_it->second.param_count) {
                    const auto &loc = alias_node->location();
                    size_t hw = alias.size() + 1 + callee.size();
                    emitError(loc, "method '" + mangled + "' expects " +
                              std::to_string(func_it->second.param_count) +
                              " argument(s), got " + std::to_string(actual_args), hw);
                }
                checkCallArgs(args_code);
                return;
            }
        }

        // Fall back to import-qualified call
        checkImportedCall(alias_node, alias, callee, args_code);
    }

    void checkImportedCall(const pgcodes::GCode *alias_node,
                           const std::string &alias,
                           const std::string &callee,
                           const pgcodes::GCode *args_code) {
        const auto &loc = alias_node->location();
        // Highlight width covers "alias.callee"
        size_t hw = alias.size() + 1 + callee.size();
        if (_current_imports == nullptr ||
            _current_imports->count(alias) == 0) {
            emitError(loc, "undefined import alias '" + alias + "'", alias.size());
            checkCallArgs(args_code);
            return;
        }

        const std::string &target_module = _current_imports->at(alias);
        auto mod_it = _module_functions.find(target_module);
        if (mod_it == _module_functions.end()) {
            emitError(loc, "imported module for alias '" + alias +
                      "' has no function table", alias.size());
            checkCallArgs(args_code);
            return;
        }
        auto func_it = mod_it->second.find(callee);
        if (func_it == mod_it->second.end()) {
            emitError(loc, "undefined function '" + callee +
                      "' in imported module '" + alias + "'", hw);
            checkCallArgs(args_code);
            return;
        }

        size_t actual_args = countArgs(args_code);
        if (actual_args != func_it->second.param_count) {
            emitError(loc, "function '" + alias + "." + callee + "' expects " +
                      std::to_string(func_it->second.param_count) +
                      " argument(s), got " + std::to_string(actual_args), hw);
        }
        checkCallArgs(args_code);
    }

    void checkIdentifierRef(const pgcodes::GCode *node,
                            const std::string &name) {
        // Skip keywords that appear as identifiers in GCode.
        if (name == "return" || name == "true" || name == "false" ||
            name == "nil" || name == "null" ||
            name == "break" || name == "continue") {
            return;
        }
        // Skip builtins (they're called, not referenced as variables usually,
        // but handle gracefully).
        if (isBuiltin(name)) return;
        // Skip struct type names.
        if (_struct_names.count(name) != 0) return;
        // Skip function names (they might appear as identifiers in some AST shapes).
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it != _module_functions.end() &&
            mod_it->second.count(name) != 0) {
            return;
        }
        // Check import aliases.
        if (_current_imports != nullptr && _current_imports->count(name) != 0) {
            return;
        }
        if (_defined_vars.count(name) == 0) {
            emitError(node->location(), "undefined variable '" + name + "'",
                      name.size());
        }
    }

    // Check struct literal field values (skip field names, only check value expressions).
    void checkStructLiteral(const pgcodes::GCode *body) {
        if (body == nullptr) return;
        // body is a tree of comma-separated `:` nodes: field: value, field: value
        if (body->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            body->getOper() == ",") {
            checkStructLiteral(body->getLeft());
            checkStructLiteral(body->getRight());
        } else if (body->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                   body->getOper() == ":") {
            // Left is field name (skip), right is value expression (check)
            if (body->getRight() != nullptr) {
                checkExpression(body->getRight());
            }
        } else {
            // Single expression (shouldn't happen in well-formed struct literal)
            checkExpression(body);
        }
    }

    void emitError(const lexer::SourceLocation &loc, const std::string &detail,
                   size_t highlight_width = 1) {
        if (loc.valid()) {
            _result.addError(
                lexer::formatDiagnostic(loc, detail, highlight_width));
        } else {
            std::string source_path;
            for (const auto &unit : _program.packages) {
                if (unit.module_id == _current_module_id) {
                    source_path = unit.source_path;
                    break;
                }
            }
            std::string prefix = source_path.empty() ? "" : source_path + ": ";
            _result.addError(prefix + "error: in function '" +
                             _current_func_name + "': " + detail);
        }
    }

    const llvm_backend::Program                &_program;
    ModuleTable                                 _module_functions;
    CheckResult                                 _result;
    std::string                                 _current_module_id;
    std::string                                 _current_func_name;
    const std::map<std::string, std::string>   *_current_imports = nullptr;
    std::set<std::string>                       _defined_vars;
    std::set<std::string>                       _struct_names;
};

} // namespace

CheckResult checkProgram(const llvm_backend::Program &program) {
    ProgramChecker checker(program);
    return checker.run();
}

} // namespace sema
} // namespace pangu
