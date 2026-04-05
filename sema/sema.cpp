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

// Type categories for compatibility checking.
// int/bool/enum are all i32 and freely interchangeable.
// string and ptr are both pointers but semantically different.
enum class TypeCat { INT, STRING, PTR, STRUCT, VOID, UNKNOWN };

TypeCat categorize(const std::string &type_name) {
    if (type_name.empty() || type_name == "void")   return TypeCat::VOID;
    if (type_name == "int" || type_name == "bool" || type_name == "char")
        return TypeCat::INT;
    if (type_name == "string")                       return TypeCat::STRING;
    if (type_name == "ptr" || type_name == "func" ||
        type_name == "DynArray" || type_name == "DynStrArray" ||
        type_name == "HashMap" || type_name == "IntMap" ||
        type_name == "StringBuilder")
                                                     return TypeCat::PTR;
    return TypeCat::STRUCT; // struct or enum name
}

// Two types are compatible if they can be used interchangeably.
bool typesCompatible(const std::string &a, const std::string &b) {
    if (a == b) return true;
    if (a.empty() || b.empty()) return true;              // unknown → compatible
    if (a == "unknown" || b == "unknown") return true;
    auto ca = categorize(a), cb = categorize(b);
    if (ca == TypeCat::UNKNOWN || cb == TypeCat::UNKNOWN) return true;
    if (ca == TypeCat::VOID || cb == TypeCat::VOID)       return true;
    // int/bool/enum are all i32
    if (ca == TypeCat::INT && cb == TypeCat::INT)          return true;
    // func/ptr are compatible with int (function pointers passed as int params)
    if ((ca == TypeCat::INT && cb == TypeCat::PTR) ||
        (ca == TypeCat::PTR && cb == TypeCat::INT))        return true;
    // string is a kind of ptr
    if ((ca == TypeCat::STRING || ca == TypeCat::PTR) &&
        (cb == TypeCat::STRING || cb == TypeCat::PTR))     return true;
    return ca == cb && a == b; // struct types must match by name
}

struct FunctionSig {
    std::string              name;
    size_t                   param_count = 0;
    std::vector<std::string> param_types;    // type name per param
    std::string              return_type;    // "int", "string", struct name, etc.
    std::vector<std::string> return_types;   // for multi-return functions
    bool                     is_generic = false;
    std::vector<std::string> type_params;    // type parameter names (e.g. ["T", "U"])
};

// Struct field info for type inference of field access.
struct StructFieldInfo {
    std::string field_name;
    std::string type_name;
};

using FunctionTable = std::map<std::string, FunctionSig>;
using StructFieldMap = std::map<std::string, std::vector<StructFieldInfo>>;
using ModuleTable   = std::map<std::string, FunctionTable>;

const std::set<std::string> BUILTIN_FUNCTIONS = {
    "println", "print", "exit", "panic", "system",
    "str_concat", "str_len", "str_eq", "str_substr",
    "str_index_of", "str_starts_with", "str_ends_with", "str_replace",
    "str_contains", "str_trim", "str_to_upper", "str_to_lower",
    "str_split", "str_repeat", "str_count", "str_replace_all",
    "int_to_str", "str_to_int",
    "read_file", "write_file",
    "str_char_at", "char_to_str",
    "make_array", "array_get", "array_set",
    "make_str_array", "str_array_get", "str_array_set",
    "args", "args_count",
    "find_pgl_files", "is_directory",
    // Pipeline builtins
    "pipeline_create", "pipeline_destroy",
    "pipeline_cache_append", "pipeline_cache_str", "pipeline_cache_reset",
    "pipeline_emit", "pipeline_output_count", "pipeline_output_get",
    "pipeline_set_worker", "pipeline_get_worker",
    "reflect_type_count", "reflect_type_name",
    "reflect_field_count", "reflect_field_name", "reflect_field_type",
    "reflect_annotation_count", "reflect_annotation_key",
    "reflect_annotation_value", "reflect_annotation_field_index",
    // HashMap (string→string)
    "make_map", "map_set", "map_get", "map_has", "map_size", "map_delete",
    // IntMap (string→int)
    "make_int_map", "int_map_set", "int_map_get", "int_map_has", "int_map_size",
    // Dynamic array (int)
    "make_dyn_array", "dyn_array_push", "dyn_array_get",
    "dyn_array_set", "dyn_array_size", "dyn_array_pop",
    // Dynamic string array
    "make_dyn_str_array", "dyn_str_array_push", "dyn_str_array_get",
    "dyn_str_array_set", "dyn_str_array_size",
    // String builder
    "make_str_builder", "sb_append", "sb_append_int", "sb_append_char",
    "sb_build", "sb_reset", "sb_len",
    // Universal
    "len", "str", "chr",
};

bool isBuiltin(const std::string &name) {
    return BUILTIN_FUNCTIONS.count(name) != 0;
}

size_t builtinParamCount(const std::string &name) {
    if (name == "println" || name == "print" || name == "exit") return 1;
    if (name == "panic") return 1;
    if (name == "system") return 1;
    if (name == "str_len" || name == "int_to_str" || name == "str_to_int")
        return 1;
    if (name == "read_file" || name == "char_to_str") return 1;
    if (name == "make_array" || name == "make_str_array") return 1;
    if (name == "args") return 1;
    if (name == "args_count") return 0;
    if (name == "str_concat" || name == "str_eq") return 2;
    if (name == "str_index_of" || name == "str_starts_with") return 2;
    if (name == "str_ends_with") return 2;
    if (name == "str_contains") return 2;
    if (name == "str_split") return 2;
    if (name == "str_count") return 2;
    if (name == "str_repeat") return 2;
    if (name == "str_trim" || name == "str_to_upper" || name == "str_to_lower") return 1;
    if (name == "str_replace_all") return 3;
    if (name == "write_file" || name == "str_char_at") return 2;
    if (name == "array_get" || name == "str_array_get") return 2;
    if (name == "find_pgl_files" || name == "is_directory") {
        if (name == "find_pgl_files") return 2;
        return 1;
    }
    if (name == "str_substr") return 3;
    if (name == "str_replace") return 3;
    if (name == "array_set" || name == "str_array_set") return 3;
    // Pipeline builtins
    if (name == "pipeline_create") return 1;
    if (name == "pipeline_destroy") return 1;
    if (name == "pipeline_cache_append") return 2;
    if (name == "pipeline_cache_str") return 1;
    if (name == "pipeline_cache_reset") return 1;
    if (name == "pipeline_emit") return 2;
    if (name == "pipeline_output_count") return 1;
    if (name == "pipeline_output_get") return 2;
    if (name == "pipeline_set_worker") return 2;
    if (name == "pipeline_get_worker") return 1;
    // Reflection builtins
    if (name == "reflect_type_count") return 0;
    if (name == "reflect_type_name") return 1;
    if (name == "reflect_field_count") return 1;
    if (name == "reflect_field_name") return 2;
    if (name == "reflect_field_type") return 2;
    if (name == "reflect_annotation_count") return 1;
    if (name == "reflect_annotation_key") return 2;
    if (name == "reflect_annotation_value") return 2;
    if (name == "reflect_annotation_field_index") return 2;
    // HashMap
    if (name == "make_map") return 0;
    if (name == "map_set") return 3;
    if (name == "map_get" || name == "map_has" || name == "map_delete") return 2;
    if (name == "map_size") return 1;
    // IntMap
    if (name == "make_int_map") return 0;
    if (name == "int_map_set") return 3;
    if (name == "int_map_get" || name == "int_map_has") return 2;
    if (name == "int_map_size") return 1;
    // Dynamic array
    if (name == "make_dyn_array") return 0;
    if (name == "dyn_array_push") return 2;
    if (name == "dyn_array_get") return 2;
    if (name == "dyn_array_set") return 3;
    if (name == "dyn_array_size" || name == "dyn_array_pop") return 1;
    // Dynamic string array
    if (name == "make_dyn_str_array") return 0;
    if (name == "dyn_str_array_push") return 2;
    if (name == "dyn_str_array_get") return 2;
    if (name == "dyn_str_array_set") return 3;
    if (name == "dyn_str_array_size") return 1;
    // String builder
    if (name == "make_str_builder") return 0;
    if (name == "sb_append") return 2;
    if (name == "sb_append_int") return 2;
    if (name == "sb_append_char") return 2;
    if (name == "sb_build") return 1;
    if (name == "sb_reset") return 1;
    if (name == "sb_len") return 1;
    if (name == "len") return 1;
    if (name == "str") return 1;
    if (name == "chr") return 1;
    return 0;
}

std::string builtinReturnType(const std::string &name) {
    // Functions returning string
    if (name == "str_concat" || name == "str_substr" || name == "str_replace" ||
        name == "str_replace_all" || name == "str_trim" ||
        name == "str_to_upper" || name == "str_to_lower" || name == "str_repeat" ||
        name == "int_to_str" || name == "char_to_str" || name == "read_file" ||
        name == "args" || name == "str_array_get" || name == "pipeline_cache_str" ||
        name == "pipeline_output_get" || name == "reflect_type_name" ||
        name == "reflect_field_name" || name == "reflect_field_type" ||
        name == "reflect_annotation_key" || name == "reflect_annotation_value") {
        return "string";
    }
    // Functions returning ptr
    if (name == "make_array" || name == "make_str_array" ||
        name == "pipeline_create" || name == "find_pgl_files") {
        return "ptr";
    }
    // Functions returning int
    if (name == "str_len" || name == "str_to_int" || name == "str_eq" ||
        name == "str_index_of" || name == "str_starts_with" || name == "str_ends_with" ||
        name == "str_contains" || name == "str_count" ||
        name == "str_char_at" || name == "array_get" || name == "args_count" ||
        name == "pipeline_output_count" || name == "pipeline_get_worker" ||
        name == "is_directory" || name == "system" ||
        name == "reflect_type_count" || name == "reflect_field_count" ||
        name == "reflect_annotation_count" || name == "reflect_annotation_field_index") {
        return "int";
    }
    // HashMap
    if (name == "make_map") return "HashMap";
    if (name == "make_int_map") return "IntMap";
    if (name == "map_get") return "string";
    if (name == "map_has" || name == "map_size") return "int";
    if (name == "map_keys" || name == "int_map_keys") return "DynStrArray";
    if (name == "int_map_get" || name == "int_map_has" || name == "int_map_size") return "int";
    // Dynamic array
    if (name == "make_dyn_array") return "DynArray";
    if (name == "make_dyn_str_array") return "DynStrArray";
    if (name == "str_split") return "DynStrArray";
    if (name == "make_str_builder") return "StringBuilder";
    if (name == "sb_build") return "string";
    if (name == "sb_len") return "int";
    if (name == "dyn_array_get" || name == "dyn_array_size" || name == "dyn_array_pop") return "int";
    if (name == "dyn_str_array_get") return "string";
    if (name == "dyn_str_array_size") return "int";
    if (name == "len") return "int";
    if (name == "str") return "string";
    if (name == "chr") return "string";
    // Void-like (side effects only)
    return "";
}

std::vector<std::string> builtinParamTypes(const std::string &name) {
    // println/print accept any type — use "unknown" as wildcard
    if (name == "println" || name == "print") return {"unknown"};
    if (name == "len") return {"unknown"};  // polymorphic
    if (name == "str") return {"int"};
    if (name == "chr") return {"int"};
    if (name == "exit" || name == "panic") return {"int"};
    if (name == "system") return {"string"};
    if (name == "str_len") return {"string"};
    if (name == "int_to_str") return {"int"};
    if (name == "str_to_int") return {"string"};
    if (name == "read_file") return {"string"};
    if (name == "char_to_str") return {"int"};
    if (name == "make_array" || name == "make_str_array") return {"int"};
    if (name == "args") return {"int"};
    if (name == "str_concat") return {"string", "string"};
    if (name == "str_eq") return {"string", "string"};
    if (name == "str_index_of") return {"string", "string"};
    if (name == "str_starts_with") return {"string", "string"};
    if (name == "str_ends_with") return {"string", "string"};
    if (name == "str_contains") return {"string", "string"};
    if (name == "str_split") return {"string", "string"};
    if (name == "str_count") return {"string", "string"};
    if (name == "str_repeat") return {"string", "int"};
    if (name == "str_trim") return {"string"};
    if (name == "str_to_upper") return {"string"};
    if (name == "str_to_lower") return {"string"};
    if (name == "str_replace_all") return {"string", "string", "string"};
    if (name == "write_file") return {"string", "string"};
    if (name == "str_char_at") return {"string", "int"};
    if (name == "array_get") return {"ptr", "int"};
    if (name == "str_array_get") return {"ptr", "int"};
    if (name == "array_set") return {"ptr", "int", "int"};
    if (name == "str_array_set") return {"ptr", "int", "string"};
    if (name == "str_substr") return {"string", "int", "int"};
    if (name == "str_replace") return {"string", "string", "string"};
    if (name == "find_pgl_files") return {"string", "ptr"};
    if (name == "is_directory") return {"string"};
    // Pipeline builtins — use ptr for pipeline handle, int for others
    if (name == "pipeline_create") return {"int"};
    if (name == "pipeline_destroy") return {"ptr"};
    if (name == "pipeline_cache_append") return {"ptr", "int"};
    if (name == "pipeline_cache_str") return {"ptr"};
    if (name == "pipeline_cache_reset") return {"ptr"};
    if (name == "pipeline_emit") return {"ptr", "string"};
    if (name == "pipeline_output_count") return {"ptr"};
    if (name == "pipeline_output_get") return {"ptr", "int"};
    if (name == "pipeline_set_worker") return {"ptr", "int"};
    if (name == "pipeline_get_worker") return {"ptr"};
    // Reflection
    if (name == "reflect_type_name") return {"int"};
    if (name == "reflect_field_count") return {"string"};
    if (name == "reflect_field_name") return {"string", "int"};
    if (name == "reflect_field_type") return {"string", "int"};
    if (name == "reflect_annotation_count") return {"string"};
    if (name == "reflect_annotation_key") return {"string", "int"};
    if (name == "reflect_annotation_value") return {"string", "int"};
    if (name == "reflect_annotation_field_index") return {"string", "int"};
    // HashMap
    if (name == "map_set") return {"ptr", "string", "string"};
    if (name == "map_get" || name == "map_has" || name == "map_delete") return {"ptr", "string"};
    if (name == "map_size") return {"ptr"};
    // IntMap
    if (name == "int_map_set") return {"ptr", "string", "int"};
    if (name == "int_map_get" || name == "int_map_has") return {"ptr", "string"};
    if (name == "int_map_size") return {"ptr"};
    // Dynamic array
    if (name == "dyn_array_push") return {"ptr", "int"};
    if (name == "dyn_array_get") return {"ptr", "int"};
    if (name == "dyn_array_set") return {"ptr", "int", "int"};
    if (name == "dyn_array_size" || name == "dyn_array_pop") return {"ptr"};
    // Dynamic string array
    if (name == "dyn_str_array_push") return {"ptr", "string"};
    if (name == "dyn_str_array_get") return {"ptr", "int"};
    if (name == "dyn_str_array_set") return {"ptr", "int", "string"};
    if (name == "dyn_str_array_size") return {"ptr"};
    // String builder
    if (name == "sb_append") return {"ptr", "string"};
    if (name == "sb_append_int") return {"ptr", "int"};
    if (name == "sb_append_char") return {"ptr", "int"};
    if (name == "sb_build" || name == "sb_reset" || name == "sb_len") return {"ptr"};
    return {};
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
                FunctionSig sig;
                sig.name        = name;
                sig.param_count = func->params.size();
                sig.is_generic  = func->isGeneric();
                sig.type_params = func->typeParams();
                // Collect parameter types.
                for (const auto &pname : func->params.orderedNames()) {
                    const auto *var = func->params.getVariable(pname);
                    sig.param_types.push_back(
                        var && var->getType() ? var->getType()->name() : "");
                }
                // Collect return type.
                if (func->result.size() == 1) {
                    const auto *rv = func->result.getVariable(
                        func->result.orderedNames().front());
                    sig.return_type =
                        rv && rv->getType() ? rv->getType()->name() : "";
                    sig.return_types.push_back(sig.return_type);
                } else if (func->result.size() > 1) {
                    for (const auto &rn : func->result.orderedNames()) {
                        const auto *rv = func->result.getVariable(rn);
                        std::string rt = rv && rv->getType() ? rv->getType()->name() : "";
                        sig.return_types.push_back(rt);
                    }
                    sig.return_type = sig.return_types.front();
                }
                table[name] = std::move(sig);
            }
            _module_functions[unit.module_id] = std::move(table);

            // Collect struct type names and field types.
            for (const auto &it : unit.package->structs.items()) {
                _struct_names.insert(it.first);
                std::vector<StructFieldInfo> fields;
                const auto *gs = it.second.get();
                for (const auto &fname : gs->orderedNames()) {
                    const auto *fvar = gs->getVariable(fname);
                    std::string ftype =
                        (fvar && fvar->getType()) ? fvar->getType()->name() : "";
                    fields.push_back({fname, ftype});
                }
                _struct_fields[it.first] = std::move(fields);
            }

            // Collect enum type names
            for (const auto &it : unit.package->type_defs.items()) {
                const auto *genum =
                    dynamic_cast<const grammer::GEnum *>(it.second.get());
                if (genum != nullptr) {
                    _enum_names.insert(genum->name());
                    if (genum->hasAssociatedData()) {
                        auto &vmap = _enum_variant_fields[genum->name()];
                        for (const auto &v : genum->variants()) {
                            std::vector<std::pair<std::string, std::string>> flds;
                            for (const auto &f : v.fields) {
                                flds.push_back({f.name, f.type});
                            }
                            vmap[v.name] = flds;
                        }
                    }
                }
                const auto *giface =
                    dynamic_cast<const grammer::GInterface *>(it.second.get());
                if (giface != nullptr) {
                    _interface_names.insert(giface->name());
                }
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
                // For generic functions, register type params as known types
                std::set<std::string> saved_generic_types;
                if (it.second->isGeneric()) {
                    saved_generic_types = _generic_type_params;
                    for (const auto &tp : it.second->typeParams()) {
                        _generic_type_params.insert(tp);
                    }
                }
                // Register parameters as defined variables with their types.
                for (const auto &pname : it.second->params.orderedNames()) {
                    const auto *var = it.second->params.getVariable(pname);
                    std::string ptype =
                        (var && var->getType()) ? var->getType()->name() : "";
                    _defined_vars[pname] = ptype;
                }
                // Track current function return type for return-value checking.
                _current_return_type = "";
                _current_return_types.clear();
                if (it.second->result.size() == 1) {
                    const auto *rv = it.second->result.getVariable(
                        it.second->result.orderedNames().front());
                    if (rv && rv->getType())
                        _current_return_type = rv->getType()->name();
                    _current_return_types.push_back(_current_return_type);
                } else if (it.second->result.size() > 1) {
                    for (const auto &rn : it.second->result.orderedNames()) {
                        const auto *rv = it.second->result.getVariable(rn);
                        std::string rt = rv && rv->getType() ? rv->getType()->name() : "";
                        _current_return_types.push_back(rt);
                    }
                    _current_return_type = _current_return_types.front();
                }
                checkStatement(it.second->code.get());
                // Restore generic type params
                if (it.second->isGeneric()) {
                    _generic_type_params = saved_generic_types;
                }
            }
        }
    }

    // Detect multi-assignment pattern: ,(a, ,(b, :=(c, expr)))
    bool isMultiAssignSema(const pgcodes::GCode *code,
                           std::vector<std::string> &names,
                           const pgcodes::GCode *&assign_node) {
        if (code == nullptr) return false;
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ",") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                names.push_back(code->getLeft()->getValue());
            } else {
                return false;
            }
            return isMultiAssignSema(code->getRight(), names, assign_node);
        }
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            (code->getOper() == ":=" || code->getOper() == "=")) {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                names.push_back(code->getLeft()->getValue());
                assign_node = code;
                return true;
            }
        }
        return false;
    }

    // ── Multi-return helpers ─────────────────────────────────────────

    void collectSemaCommaNames(const pgcodes::GCode *code,
                               std::vector<std::string> &names) {
        if (code == nullptr) return;
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ",") {
            collectSemaCommaNames(code->getLeft(), names);
            collectSemaCommaNames(code->getRight(), names);
        } else if (code->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            names.push_back(code->getValue());
        }
    }

    void inferMultiReturnTypes(const pgcodes::GCode *code,
                               std::vector<std::string> &ret_types) {
        if (code == nullptr) return;
        // Expect a function call: oper=="(" with left=identifier
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "(" &&
            code->getLeft() != nullptr &&
            code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string &callee = code->getLeft()->getValue();
            // Look up function sig in current module
            auto mod_it = _module_functions.find(_current_module_id);
            if (mod_it != _module_functions.end()) {
                auto fn_it = mod_it->second.find(callee);
                if (fn_it != mod_it->second.end()) {
                    ret_types = fn_it->second.return_types;
                    return;
                }
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
        // Multi-assignment: q, r := foo()
        // AST: ,(q, :=(r, foo()))
        if (oper == ",") {
            std::vector<std::string> names;
            const pgcodes::GCode *assign_node = nullptr;
            if (isMultiAssignSema(code, names, assign_node) &&
                assign_node != nullptr) {
                bool define_new = (assign_node->getOper() == ":=");
                if (define_new) {
                    // Infer types from the called function's return types
                    std::vector<std::string> ret_types;
                    inferMultiReturnTypes(assign_node->getRight(), ret_types);
                    for (size_t i = 0; i < names.size(); ++i) {
                        _defined_vars[names[i]] =
                            (i < ret_types.size()) ? ret_types[i] : "";
                    }
                }
                checkExpression(assign_node->getRight());
                return;
            }
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
            checkStatement(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        if (oper == "for_in") {
            // for x in iterable { body }
            // left = "in"(var_ident, iterable), right = body
            const auto *inNode = code->getLeft();
            if (inNode != nullptr && inNode->getOper() == "in") {
                const auto *varNode = inNode->getLeft();
                const auto *iterNode = inNode->getRight();
                if (iterNode != nullptr) {
                    checkExpression(iterNode);
                }
                if (varNode != nullptr) {
                    // Infer loop variable type from iterable
                    std::string iter_type = inferType(iterNode);
                    if (iter_type == "DynStrArray" || iter_type == "HashMap" ||
                        iter_type == "IntMap") {
                        _defined_vars[varNode->getValue()] = "string";
                    } else {
                        _defined_vars[varNode->getValue()] = "int";
                    }
                }
            }
            checkStatement(code->getRight());
            return;
        }
        if (oper == "switch") {
            checkExpression(code->getLeft());
            checkStatement(code->getRight());
            return;
        }
        if (oper == "match") {
            checkExpression(code->getLeft());
            std::string cond_type = inferType(code->getLeft());
            checkMatchBody(code->getRight(), cond_type);
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
            // Define-assignment: left must be identifier or comma-separated ids.
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                std::string inferred = inferType(code->getRight());
                _defined_vars[code->getLeft()->getValue()] = inferred;
            } else if (code->getLeft() != nullptr &&
                       code->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                       code->getLeft()->getOper() == ",") {
                // Multi-return destructuring: a, b := foo()
                std::vector<std::string> names;
                collectSemaCommaNames(code->getLeft(), names);
                // Infer types from the called function's return types
                std::vector<std::string> ret_types;
                inferMultiReturnTypes(code->getRight(), ret_types);
                for (size_t i = 0; i < names.size(); ++i) {
                    _defined_vars[names[i]] =
                        (i < ret_types.size()) ? ret_types[i] : "";
                }
            }
            checkExpression(code->getRight());
            return;
        }
        if (oper == "=") {
            // Assignment: left must be already defined.
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                const std::string &vname = code->getLeft()->getValue();
                checkIdentifierRef(code->getLeft(), vname);
                // Check type compatibility
                auto it = _defined_vars.find(vname);
                if (it != _defined_vars.end() && !it->second.empty()) {
                    std::string rhs_type = inferType(code->getRight());
                    if (!rhs_type.empty() &&
                        !typesCompatible(rhs_type, it->second)) {
                        auto ca = categorize(rhs_type), ce = categorize(it->second);
                        if (!(ca == TypeCat::INT && _enum_names.count(it->second)) &&
                            !(ce == TypeCat::INT && _enum_names.count(rhs_type))) {
                            const auto &loc = code->location();
                            emitError(loc, "cannot assign '" + rhs_type +
                                      "' to variable '" + vname +
                                      "' of type '" + it->second + "'",
                                      vname.size());
                        }
                    }
                }
            }
            checkExpression(code->getRight());
            return;
        }
        if (oper == "+=" || oper == "-=" || oper == "*=" || oper == "/=" ||
            oper == "%=" || oper == "&=" || oper == "|=" || oper == "^=") {
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
        if (oper == "::") {
            // Enum variant: EnumName::Variant — validated in backend
            return;
        }
        if (oper == "match") {
            checkExpression(code->getLeft());
            std::string cond_type = inferType(code->getLeft());
            checkMatchBody(code->getRight(), cond_type);
            return;
        }
        if (oper == "func_expr") {
            // Lambda: left=params block, right=body block
            std::vector<std::string> param_names;
            collectLambdaParamNames(code->getLeft(), param_names);
            auto saved_vars = _defined_vars;
            for (const auto &p : param_names) {
                _defined_vars[p] = "int";
            }
            if (code->getRight() != nullptr) {
                checkStatement(code->getRight()->getRight());
            }
            _defined_vars = saved_vars;
            return;
        }
        // Generic binary/unary operator: recurse both sides + type check.
        checkExpression(code->getLeft());
        checkExpression(code->getRight());
        checkOperatorTypes(code, oper);
    }

    // ── Specific checks ──────────────────────────────────────────────

    void checkOperatorTypes(const pgcodes::GCode *code, const std::string &oper) {
        // Only check binary arithmetic/string ops
        if (code->getLeft() == nullptr || code->getRight() == nullptr) return;

        // Arithmetic: both operands must be numeric
        if (oper == "-" || oper == "*" || oper == "/" || oper == "%") {
            std::string lt = inferType(code->getLeft());
            std::string rt = inferType(code->getRight());
            if (!lt.empty() && categorize(lt) != TypeCat::INT &&
                categorize(lt) != TypeCat::UNKNOWN) {
                emitError(code->location(), "operator '" + oper +
                          "' requires integer operands, got '" + lt + "'", 1);
            }
            if (!rt.empty() && categorize(rt) != TypeCat::INT &&
                categorize(rt) != TypeCat::UNKNOWN) {
                emitError(code->location(), "operator '" + oper +
                          "' requires integer operands, got '" + rt + "'", 1);
            }
            return;
        }

        // '+' can be int+int or string+string
        if (oper == "+") {
            std::string lt = inferType(code->getLeft());
            std::string rt = inferType(code->getRight());
            if (lt.empty() || rt.empty()) return;
            auto cl = categorize(lt), cr = categorize(rt);
            if (cl == TypeCat::UNKNOWN || cr == TypeCat::UNKNOWN) return;
            // int + int or string + string OK
            if (cl == TypeCat::INT && cr == TypeCat::INT) return;
            if ((cl == TypeCat::STRING || cl == TypeCat::PTR) &&
                (cr == TypeCat::STRING || cr == TypeCat::PTR)) return;
            // Mixed string + int etc is an error
            emitError(code->location(),
                      "operator '+' cannot mix types '" + lt + "' and '" + rt + "'",
                      1);
            return;
        }
    }

    // ── Type inference ───────────────────────────────────────────────

    // Infer the type of an expression.  Returns a type name string
    // ("int", "string", "bool", struct/enum name) or "" if unknown.
    std::string inferType(const pgcodes::GCode *code) {
        if (code == nullptr) return "";

        // Literals
        if (code->getValueType() == pgcodes::ValueType::NUMBER) return "int";
        if (code->getValueType() == pgcodes::ValueType::STRING) return "string";

        // Identifier: variable ref, function call, struct literal, keyword
        if (code->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string &name = code->getValue();

            // Boolean keywords
            if (name == "true" || name == "false") return "int";

            // Suffix call: name(args...)
            if (isSuffixCallNode(code->getRight())) {
                return inferCallReturnType(name);
            }

            // Index access: name[idx]
            if (code->getRight() != nullptr &&
                code->getRight()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                code->getRight()->getOper() == "[") {
                auto it = _defined_vars.find(name);
                if (it != _defined_vars.end()) {
                    const std::string &vt = it->second;
                    if (vt == "string") return "int";
                    if (vt == "DynArray") return "int";
                    if (vt == "DynStrArray") return "string";
                }
                return "";
            }

            // Struct literal: StructName{...}
            if (code->getRight() != nullptr &&
                code->getRight()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                code->getRight()->getOper() == "{" &&
                _struct_names.count(name) != 0) {
                return name;
            }

            // Variable reference
            auto it = _defined_vars.find(name);
            if (it != _defined_vars.end()) return it->second;

            // Function reference (bare function name used as value)
            {
                auto mod_it = _module_functions.find(_current_module_id);
                if (mod_it != _module_functions.end() &&
                    mod_it->second.count(name) != 0) {
                    return "func";
                }
            }

            return "";
        }

        // Operator nodes
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) return "";
        const std::string &oper = code->getOper();

        // Parenthesized expression
        if (oper == "(") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                return inferCallReturnType(code->getLeft()->getValue());
            }
            return inferType(code->getRight());
        }

        // Struct literal: { StructName, fields... }
        if (oper == "{") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                _struct_names.count(code->getLeft()->getValue()) != 0) {
                return code->getLeft()->getValue();
            }
            return "";
        }

        // Field access: expr.field
        if (oper == ".") {
            return inferDotType(code);
        }

        // Enum variant: EnumName::Variant
        if (oper == "::") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                const std::string &ename = code->getLeft()->getValue();
                if (_enum_names.count(ename)) return ename;
            }
            return "int";
        }

        // Match expression
        if (oper == "match") {
            return inferMatchType(code->getRight());
        }

        // Lambda expression
        if (oper == "func_expr") {
            return "func";
        }

        // Arithmetic/comparison/logical operators
        if (oper == "+" || oper == "-" || oper == "*" || oper == "/" ||
            oper == "%" || oper == "&" || oper == "|" || oper == "^") {
            std::string lt = inferType(code->getLeft());
            if (oper == "+" && categorize(lt) == TypeCat::STRING) return "string";
            return "int";
        }
        if (oper == "==" || oper == "!=" || oper == "<" || oper == ">" ||
            oper == "<=" || oper == ">=" || oper == "&&" || oper == "||") {
            return "int"; // comparisons and logical ops return int/bool
        }
        if (oper == "!") return "int";
        if (oper == "++" || oper == "--") return "int";

        // Index access: expr[idx] or slice expr[start:end]
        if (oper == "[") {
            // Check for slice: [start:end]
            auto *idx = code->getRight();
            if (idx != nullptr &&
                idx->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                idx->getOper() == ":") {
                return "string";  // slice always returns string
            }
            std::string container_type = inferType(code->getLeft());
            if (container_type == "string") return "int";         // char code
            if (container_type == "DynArray") return "int";
            if (container_type == "DynStrArray") return "string";
            return "";
        }

        return "";
    }

    std::string inferCallReturnType(const std::string &name) {
        if (isBuiltin(name)) return builtinReturnType(name);
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it != _module_functions.end()) {
            auto func_it = mod_it->second.find(name);
            if (func_it != mod_it->second.end())
                return func_it->second.return_type;
        }
        return "";
    }

    std::string inferDotType(const pgcodes::GCode *code) {
        const auto *left  = code->getLeft();
        const auto *right = code->getRight();
        if (left == nullptr || right == nullptr) return "";

        // Method call: alias.func(args) or StructName.method(args)
        if (left->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string &alias = left->getValue();

            // Extract callee for return type lookup
            std::string callee;
            if (right->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                right->getOper() == "(" && right->getLeft() != nullptr &&
                right->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                callee = right->getLeft()->getValue();
            } else if (right->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                       isSuffixCallNode(right->getRight())) {
                callee = right->getValue();
            }

            if (!callee.empty()) {
                // Struct method: StructName.method
                std::string mangled = alias + "." + callee;
                auto mod_it = _module_functions.find(_current_module_id);
                if (mod_it != _module_functions.end()) {
                    auto func_it = mod_it->second.find(mangled);
                    if (func_it != mod_it->second.end())
                        return func_it->second.return_type;
                }
                // Imported function: alias.func
                if (_current_imports != nullptr &&
                    _current_imports->count(alias) != 0) {
                    const auto &target_mod = _current_imports->at(alias);
                    auto tmod_it = _module_functions.find(target_mod);
                    if (tmod_it != _module_functions.end()) {
                        auto tfn_it = tmod_it->second.find(callee);
                        if (tfn_it != tmod_it->second.end())
                            return tfn_it->second.return_type;
                    }
                }
            }

            // Field access: var.field → look up struct field type
            if (right->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                !isSuffixCallNode(right->getRight())) {
                std::string owner_type = inferType(left);
                auto sf_it = _struct_fields.find(owner_type);
                if (sf_it != _struct_fields.end()) {
                    const std::string &field_name = right->getValue();
                    for (const auto &fi : sf_it->second) {
                        if (fi.field_name == field_name)
                            return fi.type_name;
                    }
                }
            }

            // Instance method call: var.method(args) — infer from semantic type
            if (!callee.empty()) {
                std::string owner_type = inferType(left);
                std::string ret = inferMethodReturnType(owner_type, callee);
                if (!ret.empty()) return ret;
            }
        }
        return "";
    }

    // Infer return type of a method call on a given owner type
    std::string inferMethodReturnType(const std::string &owner_type,
                                       const std::string &method) {
        if (owner_type == "HashMap") {
            if (method == "keys") return "DynStrArray";
            if (method == "get") return "string";
        }
        if (owner_type == "IntMap") {
            if (method == "keys") return "DynStrArray";
            if (method == "get") return "int";
        }
        if (owner_type == "DynArray") {
            if (method == "get") return "int";
            if (method == "size") return "int";
        }
        if (owner_type == "DynStrArray") {
            if (method == "get") return "string";
            if (method == "size") return "int";
        }
        if (owner_type == "StringBuilder") {
            if (method == "build") return "string";
            if (method == "len") return "int";
        }
        if (owner_type == "string") {
            if (method == "len") return "int";
            if (method == "char_at") return "int";
            if (method == "index_of") return "int";
            if (method == "substr") return "string";
            if (method == "starts_with") return "int";
            if (method == "ends_with") return "int";
            if (method == "contains") return "int";
            if (method == "count") return "int";
            if (method == "replace") return "string";
            if (method == "replace_all") return "string";
            if (method == "trim") return "string";
            if (method == "to_upper") return "string";
            if (method == "to_lower") return "string";
            if (method == "split") return "DynStrArray";
            if (method == "repeat") return "string";
            if (method == "eq") return "int";
            if (method == "concat") return "string";
        }
        return "";
    }

    std::string inferMatchType(const pgcodes::GCode *body) {
        if (body == nullptr) return "";
        if (body->getValueType() == pgcodes::ValueType::NOT_VALUE) {
            if (body->getOper() == "{") return inferMatchType(body->getRight());
            if (body->getOper() == ";") {
                std::string t = inferMatchType(body->getLeft());
                return t.empty() ? inferMatchType(body->getRight()) : t;
            }
            if (body->getOper() == "=>") return inferType(body->getRight());
        }
        return "";
    }

    // ── Call/reference checks ────────────────────────────────────────

    void checkReturnType(const pgcodes::GCode *node,
                         const pgcodes::GCode *expr) {
        if (_current_return_type.empty()) return;
        std::string actual = inferType(expr);
        if (actual.empty()) return;
        // Known parser quirk: `return expr.field` is parsed as
        // `(return(expr)).field`, so the sema sees just `expr` as the
        // return value.  When the inferred type is a struct but the
        // expected return type is primitive, it is almost certainly a
        // false positive — skip.
        if (_struct_names.count(actual) && categorize(_current_return_type) != TypeCat::STRUCT)
            return;
        if (!typesCompatible(actual, _current_return_type)) {
            auto ca = categorize(actual), ce = categorize(_current_return_type);
            if (ca == TypeCat::INT && _enum_names.count(_current_return_type)) return;
            if (ce == TypeCat::INT && _enum_names.count(actual)) return;
            // Try to get location from the return node or the expression.
            auto loc = node->location();
            if (!loc.valid() && expr != nullptr) loc = expr->location();
            emitError(loc, "function '" + _current_func_name +
                      "' return type is '" + _current_return_type +
                      "', but returning '" + actual + "'", 6);
        }
    }

    void checkCallArgTypes(const pgcodes::GCode *args_code,
                           const std::vector<std::string> &param_types,
                           const std::string &func_name,
                           const lexer::SourceLocation &loc,
                           size_t name_width) {
        if (args_code == nullptr) return;
        std::vector<const pgcodes::GCode *> nodes;
        collectArgNodes(args_code, nodes);
        for (size_t i = 0; i < nodes.size() && i < param_types.size(); ++i) {
            checkExpression(nodes[i]);
            std::string actual   = inferType(nodes[i]);
            const std::string &expected = param_types[i];
            if (!actual.empty() && !expected.empty() &&
                !typesCompatible(actual, expected)) {
                // For enums, int/bool compatibility — be lenient
                auto ca = categorize(actual), ce = categorize(expected);
                if (ca == TypeCat::INT && _enum_names.count(expected)) continue;
                if (ce == TypeCat::INT && _enum_names.count(actual))   continue;

                emitError(loc, "argument " + std::to_string(i + 1) +
                          " of '" + func_name + "' expects type '" +
                          expected + "', got '" + actual + "'", name_width);
            }
        }
        // Check remaining args not covered above
        for (size_t i = param_types.size(); i < nodes.size(); ++i) {
            checkExpression(nodes[i]);
        }
    }

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
            checkReturnType(node, args_code);
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
            } else {
                // Check arg types for builtins
                auto ptypes = builtinParamTypes(name);
                checkCallArgTypes(args_code, ptypes, name, loc, name_width);
                return;
            }
            checkCallArgs(args_code);
            return;
        }

        // Look up in current module.
        auto mod_it = _module_functions.find(_current_module_id);
        if (mod_it == _module_functions.end()) return;
        auto func_it = mod_it->second.find(name);
        if (func_it == mod_it->second.end()) {
            // Allow calling variables that hold function references
            auto var_it = _defined_vars.find(name);
            if (var_it != _defined_vars.end()) {
                checkCallArgs(args_code);
                return;
            }
            // Allow interface wrapping: InterfaceName(concrete_value)
            if (_interface_names.count(name) != 0) {
                checkCallArgs(args_code);
                return;
            }
            emitError(loc, "undefined function '" + name + "'", name_width);
            checkCallArgs(args_code);
            return;
        }
        if (actual_args != func_it->second.param_count) {
            emitError(loc, "function '" + name + "' expects " +
                      std::to_string(func_it->second.param_count) +
                      " argument(s), got " + std::to_string(actual_args),
                      name_width);
            checkCallArgs(args_code);
            return;
        }
        // For generic functions, skip type checking (types are parameterized)
        if (func_it->second.is_generic) {
            checkCallArgs(args_code);
            return;
        }
        // Check arg types for user-defined functions
        checkCallArgTypes(args_code, func_it->second.param_types,
                          name, loc, name_width);
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

        // If alias is a local variable, it might be an interface method call
        if (_defined_vars.count(alias) != 0) {
            checkCallArgs(args_code);
            return;
        }

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

    void collectLambdaParamNames(const pgcodes::GCode *code,
                                 std::vector<std::string> &names) {
        if (code == nullptr) return;
        // params block: oper="(", left=")", right=comma-separated identifiers
        if (code->getOper() == "(" && code->getRight() != nullptr) {
            collectLambdaParamNamesInner(code->getRight(), names);
        }
    }

    void collectLambdaParamNamesInner(const pgcodes::GCode *code,
                                      std::vector<std::string> &names) {
        if (code == nullptr) return;
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            names.push_back(code->getValue());
            return;
        }
        if (code->getOper() == ",") {
            collectLambdaParamNamesInner(code->getLeft(), names);
            collectLambdaParamNamesInner(code->getRight(), names);
        }
    }

    void checkIdentifierRef(const pgcodes::GCode *node,
                            const std::string &name) {
        // Skip keywords that appear as identifiers in GCode.
        if (name == "return" || name == "true" || name == "false" ||
            name == "nil" || name == "null" ||
            name == "break" || name == "continue" ||
            name == "CONTINUE" || name == "FINISH" || name == "TRANSFER_FINISH" ||
            name == "_") {
            return;
        }
        // Skip builtins (they're called, not referenced as variables usually,
        // but handle gracefully).
        if (isBuiltin(name)) return;
        // Skip struct type names.
        if (_struct_names.count(name) != 0) return;
        // Skip enum type names.
        if (_enum_names.count(name) != 0) return;
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

    // Check match expression body: `{ val => expr; val => expr; _ => expr; }`
    // The body is a tree of `;`-separated `=>` nodes.
    // cond_type is the type of the match condition, used for data enum destructuring.
    void checkMatchBody(const pgcodes::GCode *body,
                        const std::string &cond_type = "") {
        if (body == nullptr) return;
        if (body->getValueType() == pgcodes::ValueType::NOT_VALUE) {
            const std::string &op = body->getOper();
            if (op == "{") {
                checkMatchBody(body->getRight(), cond_type);
                return;
            }
            if (op == ";") {
                checkMatchBody(body->getLeft(), cond_type);
                checkMatchBody(body->getRight(), cond_type);
                return;
            }
            if (op == "=>") {
                // Left is pattern, right is result expression
                // For data enum patterns like Ok(v), add bindings before checking body
                auto *pattern = body->getLeft();
                std::vector<std::string> bound_names;
                if (!cond_type.empty()) {
                    collectMatchBindings(pattern, cond_type, bound_names);
                }
                auto saved_vars = _defined_vars;
                for (const auto &bname : bound_names) {
                    _defined_vars[bname] = "int"; // approximate type
                }
                checkExpression(body->getRight());
                _defined_vars = saved_vars;
                return;
            }
        }
        checkExpression(body);
    }

    // Collect binding names from a match arm pattern for data enum destructuring
    void collectMatchBindings(const pgcodes::GCode *pattern,
                              const std::string &cond_type,
                              std::vector<std::string> &bindings) {
        if (pattern == nullptr) return;
        auto eit = _enum_variant_fields.find(cond_type);
        if (eit == _enum_variant_fields.end()) return;

        std::string variant_name;
        const pgcodes::GCode *args_node = nullptr;

        // Pattern: VariantName(binding1, binding2, ...)
        if (pattern->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            variant_name = pattern->getValue();
            if (pattern->getRight() != nullptr &&
                pattern->getRight()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                pattern->getRight()->getOper() == "(") {
                args_node = pattern->getRight()->getRight();
            }
        }
        // Wrapped form: (VariantName)(binding)
        else if (pattern->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                 pattern->getOper() == "(") {
            auto *inner = pattern->getLeft();
            if (inner != nullptr &&
                inner->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                variant_name = inner->getValue();
            }
            if (pattern->getRight() != nullptr) {
                auto *rhs = pattern->getRight();
                if (rhs->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                    rhs->getOper() == "(") {
                    args_node = rhs->getRight();
                } else {
                    args_node = rhs;
                }
            }
        }

        if (variant_name.empty()) return;
        if (eit->second.count(variant_name) == 0) return;

        // Collect identifier names from args (comma-separated)
        collectIdentNames(args_node, bindings);
    }

    void collectIdentNames(const pgcodes::GCode *node,
                           std::vector<std::string> &names) {
        if (node == nullptr) return;
        if (node->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            names.push_back(node->getValue());
            return;
        }
        if (node->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            node->getOper() == ",") {
            collectIdentNames(node->getLeft(), names);
            collectIdentNames(node->getRight(), names);
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
    std::string                                 _current_return_type;
    std::vector<std::string>                     _current_return_types;
    const std::map<std::string, std::string>   *_current_imports = nullptr;
    std::map<std::string, std::string>          _defined_vars;   // name → type
    std::set<std::string>                       _struct_names;
    std::set<std::string>                       _enum_names;
    // enum name → variant name → fields (name, type)
    std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> _enum_variant_fields;
    std::set<std::string>                       _interface_names;
    std::set<std::string>                       _generic_type_params; // active type params
    StructFieldMap                              _struct_fields;
};

} // namespace

CheckResult checkProgram(const llvm_backend::Program &program) {
    ProgramChecker checker(program);
    return checker.run();
}

} // namespace sema
} // namespace pangu
