#include "llvm_backend/llvm_backend.h"

#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
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
        , _program(program)
        , _source_path(source_path) {
        _module->setSourceFileName(source_path);
        initDebugInfo(source_path);
    }

    void buildAllFunctions() {
        declareArgcArgvGlobals();
        declareStructTypes();
        declareEnumTypes();
        collectPipelineMetadata();
        emitTypeMetadata();
        declareRuntimeHelpers();
        declareFunctions();
        defineFunctions();
        emitPipelineFunctions();
        emitJitDebugRegistration();
        if (_declared_functions.count(
                functionKey(_program.entry_module_id, "main")) == 0) {
            throw std::runtime_error("main function not found");
        }
        finalizeDebugInfo();
    }

    std::unique_ptr<llvm::LLVMContext> releaseContext() {
        return std::move(_context);
    }

    std::unique_ptr<llvm::Module> releaseModule() { return std::move(_module); }

  private:
    // Struct field metadata for codegen
    struct StructFieldInfo {
        std::string name;
        size_t      index;
    };
    struct StructInfo {
        llvm::StructType                    *llvm_type;
        std::vector<StructFieldInfo>         fields;
        std::map<std::string, size_t>        field_index; // name → index
        // Reflection metadata
        std::vector<std::vector<std::pair<std::string, std::string>>> field_annotations;
        std::vector<std::pair<std::string, std::string>> struct_annotations;
    };
    // Loop context for break/continue
    struct LoopContext {
        llvm::BasicBlock *break_block;
        llvm::BasicBlock *continue_block;
    };
    // Enum variant registry: EnumName → { VariantName → ordinal }
    struct EnumInfo {
        std::map<std::string, int> variants; // variant → ordinal value
    };
    // Pipeline auto-dispatch metadata
    struct PipelineWorkerEntry {
        std::string impl_name;    // e.g., "WIdent"
        std::string process_func; // e.g., "WIdent.process"
        int         enum_ordinal; // ordinal in WorkerID enum
    };
    struct PipelineInfo {
        std::string name;         // pipeline name, e.g., "CharToToken"
        std::string module_id;
        std::vector<PipelineWorkerEntry> workers;
    };

    void declareArgcArgvGlobals() {
        _argc_global = new llvm::GlobalVariable(
            *_module, _builder.getInt32Ty(), false,
            llvm::GlobalVariable::InternalLinkage,
            llvm::ConstantInt::get(_builder.getInt32Ty(), 0),
            "__pangu_argc");
        _argv_global = new llvm::GlobalVariable(
            *_module, _builder.getPtrTy(), false,
            llvm::GlobalVariable::InternalLinkage,
            llvm::ConstantPointerNull::get(
                llvm::PointerType::get(*_context, 0)),
            "__pangu_argv");
    }

    // --- DWARF Debug Info ---

    void initDebugInfo(const std::string &source_path) {
        _di_builder = std::make_unique<llvm::DIBuilder>(*_module);
        // Extract directory and filename from source_path
        std::string dir, filename;
        auto pos = source_path.rfind('/');
        if (pos != std::string::npos) {
            dir = source_path.substr(0, pos);
            filename = source_path.substr(pos + 1);
        } else {
            dir = ".";
            filename = source_path;
        }
        // Ensure filename is not empty (e.g., when compiling a directory)
        if (filename.empty()) {
            filename = dir;
            dir = ".";
        }
        _di_file = _di_builder->createFile(filename, dir);
        _di_cu = _di_builder->createCompileUnit(
            llvm::dwarf::DW_LANG_C,
            _di_file,
            "pangu",        // producer
            false,          // isOptimized
            "",             // flags
            0               // runtime version
        );
        _module->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                               llvm::DEBUG_METADATA_VERSION);
        _module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);
    }

    void finalizeDebugInfo() {
        if (_di_builder) {
            _di_builder->finalize();
        }
    }

    llvm::DIFile *getOrCreateDIFile(const std::string &filepath) {
        auto it = _di_files.find(filepath);
        if (it != _di_files.end()) return it->second;
        std::string dir, filename;
        auto pos = filepath.rfind('/');
        if (pos != std::string::npos) {
            dir = filepath.substr(0, pos);
            filename = filepath.substr(pos + 1);
        } else {
            dir = ".";
            filename = filepath;
        }
        auto *f = _di_builder->createFile(filename, dir);
        _di_files[filepath] = f;
        return f;
    }

    llvm::DISubroutineType *createDebugFunctionType() {
        // Simple: all functions return i32 and take i32 params (no type info needed for backtrace)
        llvm::SmallVector<llvm::Metadata *, 8> types;
        types.push_back(nullptr); // return type (unspecified)
        return _di_builder->createSubroutineType(
            _di_builder->getOrCreateTypeArray(types));
    }

    void attachDebugInfo(llvm::Function *func,
                         const grammer::GFunction &gfunc) {
        if (!_di_builder) return;
        const auto &loc = gfunc.location();
        llvm::DIFile *file = loc.valid() ? getOrCreateDIFile(loc.file) : _di_file;
        unsigned line = loc.valid() ? loc.line : 0;
        auto *sp = _di_builder->createFunction(
            file,                       // scope
            gfunc.name(),               // name (PGL function name)
            "",                         // linkage name (empty → addr2line uses name)
            file,                       // file
            line,                       // line number
            createDebugFunctionType(),  // type
            line,                       // scope line
            llvm::DINode::FlagPrototyped,
            llvm::DISubprogram::SPFlagDefinition
        );
        func->setSubprogram(sp);
        _current_di_scope = sp;
    }

    void emitDebugLocation(const pgcodes::GCode *code) {
        if (!_di_builder || !_current_di_scope || !code) return;
        const auto &loc = code->location();
        if (loc.valid()) {
            _builder.SetCurrentDebugLocation(
                llvm::DILocation::get(*_context, loc.line, loc.column,
                                      _current_di_scope));
        }
    }

    void clearDebugLocation() {
        _builder.SetCurrentDebugLocation(llvm::DebugLoc());
    }

    // Declare C runtime helper functions (defined in runtime/pangu_builtins.c)
    void declareRuntimeHelpers() {
        auto *void_ty = _builder.getVoidTy();
        auto *i32_ty = _builder.getInt32Ty();
        auto *ptr_ty = _builder.getPtrTy();

        // void pg_install_signal_handlers(void)
        _module->getOrInsertFunction("pg_install_signal_handlers",
            llvm::FunctionType::get(void_ty, {}, false));
        // void pg_panic(const char* msg)
        _module->getOrInsertFunction("pg_panic",
            llvm::FunctionType::get(void_ty, {ptr_ty}, false));
        // int pg_system(const char* cmd)
        _module->getOrInsertFunction("pg_system",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));
        // int pg_str_ends_with(const char* s, const char* suffix)
        _module->getOrInsertFunction("pg_str_ends_with",
            llvm::FunctionType::get(i32_ty, {ptr_ty, ptr_ty}, false));
        // int pg_is_directory(const char* path)
        _module->getOrInsertFunction("pg_is_directory",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));
        // int pg_find_pgl_files(const char* dir, char** out_arr)
        _module->getOrInsertFunction("pg_find_pgl_files",
            llvm::FunctionType::get(i32_ty, {ptr_ty, ptr_ty}, false));
        // void pg_print_backtrace(void)
        _module->getOrInsertFunction("pg_print_backtrace",
            llvm::FunctionType::get(void_ty, {}, false));

        // Pipeline runtime helpers
        // void* pg_pipeline_create(int elem_size)
        _module->getOrInsertFunction("pg_pipeline_create",
            llvm::FunctionType::get(ptr_ty, {i32_ty}, false));
        // void pg_pipeline_destroy(void* state)
        _module->getOrInsertFunction("pg_pipeline_destroy",
            llvm::FunctionType::get(void_ty, {ptr_ty}, false));
        // void pg_pipeline_cache_append(void* state, int ch)
        _module->getOrInsertFunction("pg_pipeline_cache_append",
            llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false));
        // const char* pg_pipeline_cache_str(void* state)
        _module->getOrInsertFunction("pg_pipeline_cache_str",
            llvm::FunctionType::get(ptr_ty, {ptr_ty}, false));
        // void pg_pipeline_cache_reset(void* state)
        _module->getOrInsertFunction("pg_pipeline_cache_reset",
            llvm::FunctionType::get(void_ty, {ptr_ty}, false));
        // void pg_pipeline_emit(void* state, void* elem)
        _module->getOrInsertFunction("pg_pipeline_emit",
            llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false));
        // int pg_pipeline_output_count(void* state)
        _module->getOrInsertFunction("pg_pipeline_output_count",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));
        // void* pg_pipeline_output_get(void* state, int index)
        _module->getOrInsertFunction("pg_pipeline_output_get",
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false));
        // void pg_pipeline_set_worker(void* state, int worker_id)
        _module->getOrInsertFunction("pg_pipeline_set_worker",
            llvm::FunctionType::get(void_ty, {ptr_ty, i32_ty}, false));
        // int pg_pipeline_get_worker(void* state)
        _module->getOrInsertFunction("pg_pipeline_get_worker",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));

        // Reflection runtime helpers
        // int reflect_type_count(void)
        _module->getOrInsertFunction("reflect_type_count",
            llvm::FunctionType::get(i32_ty, {}, false));
        // const char* reflect_type_name(int index)
        _module->getOrInsertFunction("reflect_type_name",
            llvm::FunctionType::get(ptr_ty, {i32_ty}, false));
        // int reflect_field_count(const char* type_name)
        _module->getOrInsertFunction("reflect_field_count",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));
        // const char* reflect_field_name(const char* type_name, int index)
        _module->getOrInsertFunction("reflect_field_name",
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false));
        // const char* reflect_field_type(const char* type_name, int index)
        _module->getOrInsertFunction("reflect_field_type",
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false));
        // int reflect_annotation_count(const char* type_name)
        _module->getOrInsertFunction("reflect_annotation_count",
            llvm::FunctionType::get(i32_ty, {ptr_ty}, false));
        // const char* reflect_annotation_key(const char* type_name, int index)
        _module->getOrInsertFunction("reflect_annotation_key",
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false));
        // const char* reflect_annotation_value(const char* type_name, int index)
        _module->getOrInsertFunction("reflect_annotation_value",
            llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false));
        // int reflect_annotation_field_index(const char* type_name, int index)
        _module->getOrInsertFunction("reflect_annotation_field_index",
            llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty}, false));
        // void __pangu_register_types(int count, void* registry)
        _module->getOrInsertFunction("__pangu_register_types",
            llvm::FunctionType::get(void_ty, {i32_ty, ptr_ty}, false));
        // void pg_register_jit_function(void* addr, const char* name,
        //                                const char* file, int line)
        _module->getOrInsertFunction("pg_register_jit_function",
            llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty, ptr_ty, i32_ty}, false));
    }

    void declareStructTypes() {
        // Pass 1: create opaque struct types so forward references work
        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->structs.items()) {
                const auto *gs = it.second.get();
                const std::string struct_name = gs->name();
                auto *stype =
                    llvm::StructType::create(*_context, "struct." + struct_name);
                StructInfo info;
                info.llvm_type = stype;
                _struct_types[struct_name] = std::move(info);
            }
        }
        // Pass 2: fill in field types and indices
        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->structs.items()) {
                const auto *gs = it.second.get();
                const std::string struct_name = gs->name();
                auto &info = _struct_types[struct_name];

                std::vector<llvm::Type *> field_types;
                size_t idx = 0;
                for (const auto &fname : gs->orderedNames()) {
                    const auto *var = gs->getVariable(fname);
                    auto *ftype = resolveTypeName(var->getType()->name());
                    field_types.push_back(ftype);
                    info.fields.push_back(StructFieldInfo{fname, idx});
                    info.field_index[fname] = idx;
                    info.field_annotations.push_back(var->annotations());
                    ++idx;
                }
                info.llvm_type->setBody(field_types);
                info.struct_annotations = gs->annotations();
            }
        }
    }

    void declareEnumTypes() {
        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->type_defs.items()) {
                const auto *genum =
                    dynamic_cast<const grammer::GEnum *>(it.second.get());
                if (genum == nullptr) continue;
                EnumInfo info;
                int ordinal = 0;
                for (const auto &variant : genum->items()) {
                    info.variants[variant] = ordinal++;
                }
                _enum_types[genum->name()] = std::move(info);
            }
        }
    }

    // Collect pipeline metadata: match pipeline type_defs with worker impls
    // and resolve worker IDs from enum types.
    void collectPipelineMetadata() {
        for (const auto &unit : _program.packages) {
            for (const auto &td : unit.package->function_defs.items()) {
                if (td.second->getDeclKeyword() != "pipeline") continue;
                const std::string &pname = td.second->name();

                PipelineInfo info;
                info.name = pname;
                info.module_id = unit.module_id;

                // Find workers: impls with base==pname and "worker" modifier
                for (const auto &impl_pair : unit.package->impls.items()) {
                    const auto *impl = impl_pair.second.get();
                    bool is_worker = false;
                    for (const auto &mod : impl->modifiers()) {
                        if (mod == "worker") { is_worker = true; break; }
                    }
                    if (!is_worker || impl->base() != pname) continue;

                    const std::string &wname = impl->name();
                    int ordinal = -1;
                    for (const auto &[ename, einfo] : _enum_types) {
                        auto vit = einfo.variants.find(wname);
                        if (vit != einfo.variants.end()) {
                            ordinal = vit->second;
                            break;
                        }
                    }
                    if (ordinal >= 0) {
                        info.workers.push_back({wname, wname + ".process", ordinal});
                    }
                }
                if (!info.workers.empty()) {
                    _pipeline_types[pname] = std::move(info);
                }
            }
        }
    }

    // Create a global string constant (no insert point needed)
    llvm::Constant *createGlobalString(const std::string &str, const std::string &name) {
        auto *strConst = llvm::ConstantDataArray::getString(*_context, str, true);
        auto *gv = new llvm::GlobalVariable(
            *_module, strConst->getType(), true,
            llvm::GlobalVariable::PrivateLinkage, strConst, name);
        gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
        return llvm::ConstantExpr::getBitCast(gv, _builder.getPtrTy());
    }

    // Emit runtime-readable metadata for all struct and enum types.
    // Creates global constants: __pangu_type_count, __pangu_type_names[],
    // and per-type info arrays for field names, types, and annotations.
    void emitTypeMetadata() {
        auto *i32_ty = _builder.getInt32Ty();
        auto *ptr_ty = _builder.getPtrTy();

        // PanguTypeMeta struct: { ptr name, i32 field_count, ptr field_names,
        //   ptr field_types, i32 ann_count, ptr ann_keys, ptr ann_values, ptr ann_field_indices }
        auto *meta_ty = llvm::StructType::create(
            *_context, {ptr_ty, i32_ty, ptr_ty, ptr_ty, i32_ty, ptr_ty, ptr_ty, ptr_ty},
            "PanguTypeMeta");

        struct TypeMetaEntry {
            std::string name;
            int field_count;
            std::vector<std::string> field_names;
            std::vector<std::string> field_types;
            std::vector<std::string> annotation_keys;
            std::vector<std::string> annotation_values;
            std::vector<int> annotation_field_indices;
        };

        std::vector<TypeMetaEntry> entries;

        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->structs.items()) {
                const auto *gs = it.second.get();
                TypeMetaEntry entry;
                entry.name = gs->name();
                entry.field_count = (int)gs->orderedNames().size();
                for (const auto &fname : gs->orderedNames()) {
                    const auto *var = gs->getVariable(fname);
                    entry.field_names.push_back(fname);
                    entry.field_types.push_back(var->getType()->name());
                }
                auto sit = _struct_types.find(gs->name());
                if (sit != _struct_types.end()) {
                    for (size_t fi = 0; fi < sit->second.field_annotations.size(); fi++) {
                        for (const auto &ann : sit->second.field_annotations[fi]) {
                            entry.annotation_keys.push_back(ann.first);
                            entry.annotation_values.push_back(ann.second);
                            entry.annotation_field_indices.push_back((int)fi);
                        }
                    }
                }
                entries.push_back(std::move(entry));
            }
        }

        for (const auto &[name, info] : _enum_types) {
            TypeMetaEntry entry;
            entry.name = name;
            entry.field_count = (int)info.variants.size();
            for (const auto &[vname, vval] : info.variants) {
                entry.field_names.push_back(vname);
                entry.field_types.push_back("int");
            }
            entries.push_back(std::move(entry));
        }

        int type_count = (int)entries.size();
        auto *null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(*_context, 0));

        // Build each PanguTypeMeta constant
        std::vector<llvm::Constant *> meta_consts;
        for (size_t ti = 0; ti < entries.size(); ti++) {
            const auto &e = entries[ti];
            std::string pfx = "__pangu_meta_" + e.name + "_";

            auto *name_str = createGlobalString(e.name, pfx + "name");

            // Field names array
            llvm::Constant *fn_arr_ptr = null_ptr;
            if (!e.field_names.empty()) {
                std::vector<llvm::Constant *> fn_ptrs;
                for (const auto &fn : e.field_names)
                    fn_ptrs.push_back(createGlobalString(fn, pfx + "fn_" + fn));
                auto *arr_ty = llvm::ArrayType::get(ptr_ty, fn_ptrs.size());
                auto *arr_gv = new llvm::GlobalVariable(
                    *_module, arr_ty, true, llvm::GlobalVariable::PrivateLinkage,
                    llvm::ConstantArray::get(arr_ty, fn_ptrs), pfx + "field_names");
                fn_arr_ptr = llvm::ConstantExpr::getBitCast(arr_gv, ptr_ty);
            }

            // Field types array
            llvm::Constant *ft_arr_ptr = null_ptr;
            if (!e.field_types.empty()) {
                std::vector<llvm::Constant *> ft_ptrs;
                for (const auto &ft : e.field_types)
                    ft_ptrs.push_back(createGlobalString(ft, pfx + "ft_" + ft));
                auto *arr_ty = llvm::ArrayType::get(ptr_ty, ft_ptrs.size());
                auto *arr_gv = new llvm::GlobalVariable(
                    *_module, arr_ty, true, llvm::GlobalVariable::PrivateLinkage,
                    llvm::ConstantArray::get(arr_ty, ft_ptrs), pfx + "field_types");
                ft_arr_ptr = llvm::ConstantExpr::getBitCast(arr_gv, ptr_ty);
            }

            int ann_count = (int)e.annotation_keys.size();
            llvm::Constant *ak_ptr = null_ptr, *av_ptr = null_ptr, *afi_ptr = null_ptr;
            if (ann_count > 0) {
                std::vector<llvm::Constant *> ak_ptrs, av_ptrs;
                for (const auto &ak : e.annotation_keys)
                    ak_ptrs.push_back(createGlobalString(ak, pfx + "ak"));
                for (const auto &av : e.annotation_values)
                    av_ptrs.push_back(createGlobalString(av.empty() ? "" : av, pfx + "av"));

                auto *ak_arr_ty = llvm::ArrayType::get(ptr_ty, ak_ptrs.size());
                auto *ak_gv = new llvm::GlobalVariable(
                    *_module, ak_arr_ty, true, llvm::GlobalVariable::PrivateLinkage,
                    llvm::ConstantArray::get(ak_arr_ty, ak_ptrs), pfx + "ann_keys");
                ak_ptr = llvm::ConstantExpr::getBitCast(ak_gv, ptr_ty);

                auto *av_arr_ty = llvm::ArrayType::get(ptr_ty, av_ptrs.size());
                auto *av_gv = new llvm::GlobalVariable(
                    *_module, av_arr_ty, true, llvm::GlobalVariable::PrivateLinkage,
                    llvm::ConstantArray::get(av_arr_ty, av_ptrs), pfx + "ann_values");
                av_ptr = llvm::ConstantExpr::getBitCast(av_gv, ptr_ty);

                std::vector<llvm::Constant *> afi_vals;
                for (int idx : e.annotation_field_indices)
                    afi_vals.push_back(llvm::ConstantInt::get(i32_ty, idx));
                auto *afi_arr_ty = llvm::ArrayType::get(i32_ty, afi_vals.size());
                auto *afi_gv = new llvm::GlobalVariable(
                    *_module, afi_arr_ty, true, llvm::GlobalVariable::PrivateLinkage,
                    llvm::ConstantArray::get(afi_arr_ty, afi_vals), pfx + "ann_fidx");
                afi_ptr = llvm::ConstantExpr::getBitCast(afi_gv, ptr_ty);
            }

            auto *meta_val = llvm::ConstantStruct::get(meta_ty, {
                name_str,
                llvm::ConstantInt::get(i32_ty, e.field_count),
                fn_arr_ptr,
                ft_arr_ptr,
                llvm::ConstantInt::get(i32_ty, ann_count),
                ak_ptr, av_ptr, afi_ptr
            });
            meta_consts.push_back(meta_val);
        }

        // __pangu_type_count
        new llvm::GlobalVariable(
            *_module, i32_ty, true, llvm::GlobalVariable::ExternalLinkage,
            llvm::ConstantInt::get(i32_ty, type_count),
            "__pangu_type_count");

        // __pangu_type_registry array
        if (!meta_consts.empty()) {
            auto *arr_ty = llvm::ArrayType::get(meta_ty, meta_consts.size());
            auto *arr = llvm::ConstantArray::get(arr_ty, meta_consts);
            new llvm::GlobalVariable(
                *_module, arr_ty, true, llvm::GlobalVariable::ExternalLinkage,
                arr, "__pangu_type_registry");
        }
    }

    llvm::Type *resolveTypeName(const std::string &name) {
        if (name == "int")    return _builder.getInt32Ty();
        if (name == "bool")   return _builder.getInt32Ty();
        if (name == "string") return _builder.getPtrTy();
        if (name == "ptr")    return _builder.getPtrTy();
        auto it = _struct_types.find(name);
        if (it != _struct_types.end()) {
            return it->second.llvm_type;
        }
        if (_enum_types.count(name)) {
            return _builder.getInt32Ty();
        }
        throw std::runtime_error("unsupported type: " + name);
    }

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

    // Generate __pangu_register_jit_debug() that registers each PGL function's
    // address+source location with the runtime, enabling JIT-mode backtraces.
    void emitJitDebugRegistration() {
        auto *void_ty = _builder.getVoidTy();
        auto *reg_fn_type = llvm::FunctionType::get(void_ty, {}, false);
        auto *reg_fn = llvm::Function::Create(
            reg_fn_type, llvm::Function::ExternalLinkage,
            "__pangu_register_jit_debug", *_module);
        auto *entry = llvm::BasicBlock::Create(*_context, "entry", reg_fn);
        _builder.SetInsertPoint(entry);
        clearDebugLocation();

        auto reg_callee = _module->getOrInsertFunction(
            "pg_register_jit_function",
            llvm::FunctionType::get(void_ty,
                {_builder.getPtrTy(), _builder.getPtrTy(),
                 _builder.getPtrTy(), _builder.getInt32Ty()}, false));

        for (const auto &unit : _program.packages) {
            for (const auto &it : unit.package->functions.items()) {
                const auto &gfunc = *it.second;
                auto func_key = functionKey(unit.module_id, gfunc.name());
                auto func_it = _declared_functions.find(func_key);
                if (func_it == _declared_functions.end()) continue;

                llvm::Function *llvm_func = func_it->second;
                const auto &loc = gfunc.location();
                std::string name_str = gfunc.name();
                std::string file_str = loc.valid() ? loc.file : _source_path;
                int line = loc.valid() ? (int)loc.line : 0;

                auto *name_val = _builder.CreateGlobalStringPtr(name_str);
                auto *file_val = _builder.CreateGlobalStringPtr(file_str);
                auto *line_val = llvm::ConstantInt::get(
                    _builder.getInt32Ty(), line);
                _builder.CreateCall(reg_callee,
                    {llvm_func, name_val, file_val, line_val});
            }
        }
        _builder.CreateRetVoid();
    }

    // Generate LLVM IR for auto-generated pipeline __dispatch functions.
    // Each dispatch function is a switch on worker ID that forwards to
    // the corresponding Worker.process method.
    void emitPipelineFunctions() {
        for (const auto &[pname, pinfo] : _pipeline_types) {
            std::string dispatch_name = pname + ".__dispatch";
            auto fkey = functionKey(pinfo.module_id, dispatch_name);
            auto it = _declared_functions.find(fkey);
            if (it == _declared_functions.end()) continue;

            llvm::Function *dispatch_fn = it->second;
            auto *entry = llvm::BasicBlock::Create(*_context, "entry", dispatch_fn);
            _builder.SetInsertPoint(entry);
            clearDebugLocation();

            // First arg is wid, rest are forwarded to workers
            auto arg_it = dispatch_fn->arg_begin();
            llvm::Value *wid = &*arg_it++;
            std::vector<llvm::Value *> worker_args;
            while (arg_it != dispatch_fn->arg_end()) {
                worker_args.push_back(&*arg_it++);
            }

            auto *default_bb = llvm::BasicBlock::Create(
                *_context, "default", dispatch_fn);
            auto *sw = _builder.CreateSwitch(
                wid, default_bb, pinfo.workers.size());

            for (const auto &w : pinfo.workers) {
                auto wkey = functionKey(pinfo.module_id, w.process_func);
                auto wfn_it = _declared_functions.find(wkey);
                if (wfn_it == _declared_functions.end()) continue;

                auto *case_bb = llvm::BasicBlock::Create(
                    *_context, "case_" + w.impl_name, dispatch_fn);
                sw->addCase(
                    llvm::ConstantInt::get(_builder.getInt32Ty(), w.enum_ordinal),
                    case_bb);

                _builder.SetInsertPoint(case_bb);
                auto *result = _builder.CreateCall(wfn_it->second, worker_args);
                _builder.CreateRet(result);
            }

            // Default: return 0 (Signal::CONTINUE)
            _builder.SetInsertPoint(default_bb);
            _builder.CreateRet(llvm::ConstantInt::get(_builder.getInt32Ty(), 0));
        }
    }

    llvm::FunctionType *makeFunctionType(const grammer::GFunction &function) {
        if (function.result.size() > 1) {
            throw std::runtime_error("multiple return values are not supported");
        }

        std::vector<llvm::Type *> params;
        if (function.name() == "main") {
            // main(argc i32, argv i8**)
            params.push_back(_builder.getInt32Ty());
            params.push_back(_builder.getPtrTy());
        } else {
            for (const auto &name : function.params.orderedNames()) {
                const auto *var = function.params.getVariable(name);
                params.push_back(getLLVMType(*var));
            }
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
        return resolveTypeName(var.getType()->name());
    }

    void defineFunction(const PackageUnit &unit, const grammer::GFunction &function) {
        // Skip synthetic pipeline functions — they're generated by emitPipelineFunctions
        if (function.code == nullptr) return;

        _current_module_id   = unit.module_id;
        _current_imports     = &unit.import_alias_to_module;
        _current_function =
            _declared_functions.at(functionKey(unit.module_id, function.name()));
        _variables.clear();
        _terminated = false;

        // Attach DWARF debug info to this function
        attachDebugInfo(_current_function, function);

        auto *entry =
            llvm::BasicBlock::Create(*_context, "entry", _current_function);
        _builder.SetInsertPoint(entry);

        // Set initial debug location to function entry line
        if (_current_di_scope) {
            const auto &loc = function.location();
            unsigned line = loc.valid() ? loc.line : 0;
            _builder.SetCurrentDebugLocation(
                llvm::DILocation::get(*_context, line, 0, _current_di_scope));
        }

        if (function.name() == "main") {
            // Store argc/argv from main params into globals
            auto arg_it = _current_function->arg_begin();
            llvm::Value *argc_val = &*arg_it++;
            llvm::Value *argv_val = &*arg_it;
            argc_val->setName("argc");
            argv_val->setName("argv");
            _builder.CreateStore(argc_val, _argc_global);
            _builder.CreateStore(argv_val, _argv_global);

            // Install signal handlers for crash debugging
            auto install_fn = _module->getFunction("pg_install_signal_handlers");
            if (install_fn) {
                _builder.CreateCall(install_fn, {});
            }

            // Register type metadata for reflection
            auto register_fn = _module->getFunction("__pangu_register_types");
            auto *registry_gv = _module->getGlobalVariable("__pangu_type_registry");
            auto *count_gv = _module->getGlobalVariable("__pangu_type_count");
            if (register_fn && registry_gv && count_gv) {
                auto *count_val = _builder.CreateLoad(_builder.getInt32Ty(), count_gv);
                _builder.CreateCall(register_fn, {count_val, registry_gv});
            }
        } else {
            bindParameters(function);
        }
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

        // Set debug location for this statement
        emitDebugLocation(code);

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
        if (oper == "for_in") {
            return emitForInStatement(code);
        }
        if (oper == "switch") {
            return emitSwitchStatement(code);
        }
        if (oper == "match") {
            return emitMatchExpression(code);
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
        _loop_stack.push_back({end_block, cond_block});
        emitStatement(code->getRight());
        _loop_stack.pop_back();
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
        _loop_stack.push_back({end_block, step_block});
        emitStatement(code->getRight());
        _loop_stack.pop_back();
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

    // ── for x in iterable { body } ──────────────────────────────────
    // AST: for_in(left="in"(var_ident, iterable), right=body)
    // If iterable is a number N: iterate 0..N-1
    // If iterable is an identifier: iterate 0..str_len(var)-1 for strings
    //   or use array_len convention
    llvm::Value *emitForInStatement(const pgcodes::GCode *code) {
        auto *function = _current_function;
        const auto *inNode = code->getLeft();
        if (inNode == nullptr || inNode->getOper() != "in") {
            throw std::runtime_error("for_in: malformed AST, missing 'in' node");
        }
        const auto *varNode = inNode->getLeft();
        const auto *iterNode = inNode->getRight();
        if (varNode == nullptr || iterNode == nullptr) {
            throw std::runtime_error("for_in: missing variable or iterable");
        }

        std::string varName = varNode->getValue();

        // Determine iteration count
        llvm::Value *count = nullptr;
        if (iterNode->getValueType() == pgcodes::ValueType::NUMBER) {
            count = llvm::ConstantInt::get(_builder.getInt32Ty(),
                                           std::stoi(iterNode->getValue()));
        } else {
            count = emitExpression(iterNode);
        }

        // Allocate loop variable
        auto *indexAlloca = createVariableSlot(varName, _builder.getInt32Ty());
        _builder.CreateStore(
            llvm::ConstantInt::get(_builder.getInt32Ty(), 0), indexAlloca);

        // Save and register loop variable
        llvm::AllocaInst *prevAlloca = nullptr;
        auto prevIt = _variables.find(varName);
        if (prevIt != _variables.end()) {
            prevAlloca = prevIt->second;
        }
        _variables[varName] = indexAlloca;

        auto *cond_block =
            llvm::BasicBlock::Create(*_context, "forin.cond", function);
        auto *body_block = llvm::BasicBlock::Create(*_context, "forin.body");
        auto *step_block = llvm::BasicBlock::Create(*_context, "forin.step");
        auto *end_block  = llvm::BasicBlock::Create(*_context, "forin.end");

        _builder.CreateBr(cond_block);

        // Condition: index < count
        _builder.SetInsertPoint(cond_block);
        auto *idx = _builder.CreateLoad(_builder.getInt32Ty(), indexAlloca, "idx");
        auto *cmp = _builder.CreateICmpSLT(idx, count, "forin.cmp");
        _builder.CreateCondBr(cmp, body_block, end_block);

        // Body
        function->insert(function->end(), body_block);
        _builder.SetInsertPoint(body_block);
        _terminated = false;
        _loop_stack.push_back({end_block, step_block});
        emitStatement(code->getRight());
        _loop_stack.pop_back();
        if (!_terminated &&
            _builder.GetInsertBlock()->getTerminator() == nullptr) {
            _builder.CreateBr(step_block);
        }

        // Step: index++
        function->insert(function->end(), step_block);
        _builder.SetInsertPoint(step_block);
        auto *nextIdx = _builder.CreateAdd(
            _builder.CreateLoad(_builder.getInt32Ty(), indexAlloca, "idx.cur"),
            llvm::ConstantInt::get(_builder.getInt32Ty(), 1), "idx.next");
        _builder.CreateStore(nextIdx, indexAlloca);
        _builder.CreateBr(cond_block);

        // End
        function->insert(function->end(), end_block);
        _builder.SetInsertPoint(end_block);
        _terminated = false;

        // Restore previous variable binding
        if (prevAlloca != nullptr) {
            _variables[varName] = prevAlloca;
        } else {
            _variables.erase(varName);
        }

        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
    }

    // ── switch(expr) { case V: {...} ... default: {...} } ────────────
    void collectCaseNodes(const pgcodes::GCode *code,
                          std::vector<const pgcodes::GCode *> &cases) {
        if (code == nullptr) return;
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ";") {
            collectCaseNodes(code->getLeft(), cases);
            collectCaseNodes(code->getRight(), cases);
            return;
        }
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "{") {
            collectCaseNodes(code->getRight(), cases);
            return;
        }
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "case") {
            cases.push_back(code);
            return;
        }
    }

    llvm::Value *emitSwitchStatement(const pgcodes::GCode *code) {
        auto *function = _current_function;

        // Evaluate the switch condition
        auto *cond_value = emitExpression(code->getLeft());
        if (!cond_value->getType()->isIntegerTy()) {
            throw std::runtime_error("switch condition must be an integer");
        }

        // Collect case nodes from the body
        std::vector<const pgcodes::GCode *> cases;
        collectCaseNodes(code->getRight(), cases);

        auto *end_block = llvm::BasicBlock::Create(*_context, "switch.end");
        auto *default_block = end_block; // falls through to end if no default

        // Create basic blocks for each case
        std::vector<llvm::BasicBlock *> case_blocks;
        const pgcodes::GCode *default_case = nullptr;
        for (size_t i = 0; i < cases.size(); ++i) {
            if (cases[i]->getLeft() == nullptr) {
                // default case
                auto *bb = llvm::BasicBlock::Create(
                    *_context, "switch.default");
                case_blocks.push_back(bb);
                default_block = bb;
                default_case = cases[i];
            } else {
                auto *bb = llvm::BasicBlock::Create(
                    *_context, "switch.case." + std::to_string(i));
                case_blocks.push_back(bb);
            }
        }

        // Create LLVM switch instruction
        auto *switch_inst = _builder.CreateSwitch(
            cond_value, default_block,
            static_cast<unsigned>(cases.size()));

        // Add cases and emit bodies
        bool all_terminated = true;
        for (size_t i = 0; i < cases.size(); ++i) {
            if (cases[i]->getLeft() != nullptr) {
                auto *case_val = emitExpression(cases[i]->getLeft());
                auto *const_val = llvm::dyn_cast<llvm::ConstantInt>(case_val);
                if (const_val == nullptr) {
                    throw std::runtime_error(
                        "switch case value must be a constant integer");
                }
                switch_inst->addCase(const_val, case_blocks[i]);
            }

            function->insert(function->end(), case_blocks[i]);
            _builder.SetInsertPoint(case_blocks[i]);
            _terminated = false;

            // Emit case body — unwrap the `{` block
            const auto *body = cases[i]->getRight();
            emitStatement(body);

            bool block_terminated =
                _terminated ||
                _builder.GetInsertBlock()->getTerminator() != nullptr;
            if (!block_terminated) {
                _builder.CreateBr(end_block);
            }
            if (!block_terminated) {
                all_terminated = false;
            }
        }

        // The end block is needed when:
        // - Not all cases terminate (need a merge point)
        // - No default case exists (switch default target needs a valid block)
        bool need_end = !all_terminated || cases.empty() ||
                        default_case == nullptr;
        if (need_end) {
            function->insert(function->end(), end_block);
            _builder.SetInsertPoint(end_block);
            _terminated = false;
        } else {
            _terminated = true;
        }
        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
    }

    // Collect => arms from match body (traversing ; and { wrappers)
    void collectMatchArms(const pgcodes::GCode *code,
                          std::vector<const pgcodes::GCode *> &arms) {
        if (code == nullptr) return;
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ";") {
            collectMatchArms(code->getLeft(), arms);
            collectMatchArms(code->getRight(), arms);
            return;
        }
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "{") {
            collectMatchArms(code->getRight(), arms);
            return;
        }
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "=>") {
            arms.push_back(code);
            return;
        }
    }

    // match(expr) { val => body; val => body; _ => body; }
    // Lowered as an if/else chain with PHI node for result.
    llvm::Value *emitMatchExpression(const pgcodes::GCode *code) {
        auto *function = _current_function;
        auto *cond_value = emitExpression(code->getLeft());

        std::vector<const pgcodes::GCode *> arms;
        collectMatchArms(code->getRight(), arms);

        if (arms.empty()) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }

        auto *end_block = llvm::BasicBlock::Create(*_context, "match.end");
        std::vector<std::pair<llvm::Value *, llvm::BasicBlock *>> results;

        for (size_t i = 0; i < arms.size(); ++i) {
            const auto *arm = arms[i];
            const auto *pattern = arm->getLeft();
            const auto *body = arm->getRight();

            bool is_wildcard = (pattern != nullptr &&
                pattern->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                pattern->getValue() == "_");

            if (is_wildcard || pattern == nullptr) {
                _terminated = false;
                auto *val = emitExpression(body);
                if (!_terminated) {
                    results.push_back({val, _builder.GetInsertBlock()});
                    _builder.CreateBr(end_block);
                }
            } else {
                auto *pattern_val = emitExpression(pattern);
                llvm::Value *cmp;
                if (cond_value->getType()->isPointerTy() &&
                    pattern_val->getType()->isPointerTy()) {
                    auto strcmp_type = llvm::FunctionType::get(
                        _builder.getInt32Ty(),
                        {_builder.getPtrTy(), _builder.getPtrTy()}, false);
                    auto strcmp_fn = _module->getOrInsertFunction(
                        "strcmp", strcmp_type);
                    auto *cmp_result = _builder.CreateCall(
                        strcmp_fn, {cond_value, pattern_val}, "strcmp");
                    cmp = _builder.CreateICmpEQ(cmp_result,
                        llvm::ConstantInt::get(_builder.getInt32Ty(), 0),
                        "match.cmp");
                } else {
                    cmp = _builder.CreateICmpEQ(cond_value, pattern_val,
                        "match.cmp");
                }

                auto *then_block = llvm::BasicBlock::Create(
                    *_context, "match.arm." + std::to_string(i));
                auto *next_block = llvm::BasicBlock::Create(
                    *_context, "match.next." + std::to_string(i));

                _builder.CreateCondBr(cmp, then_block, next_block);

                function->insert(function->end(), then_block);
                _builder.SetInsertPoint(then_block);
                _terminated = false;
                auto *val = emitExpression(body);
                if (!_terminated) {
                    results.push_back({val, _builder.GetInsertBlock()});
                    _builder.CreateBr(end_block);
                }

                function->insert(function->end(), next_block);
                _builder.SetInsertPoint(next_block);
                _terminated = false;
            }
        }

        // If no wildcard, provide default
        if (!_builder.GetInsertBlock()->getTerminator()) {
            auto *default_val = llvm::ConstantInt::get(
                _builder.getInt32Ty(), 0);
            results.push_back({default_val, _builder.GetInsertBlock()});
            _builder.CreateBr(end_block);
        }

        function->insert(function->end(), end_block);
        _builder.SetInsertPoint(end_block);
        _terminated = false;

        if (results.empty()) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }

        auto *phi = _builder.CreatePHI(results[0].first->getType(),
                                        results.size(), "match.val");
        for (auto &[val, block] : results) {
            phi->addIncoming(val, block);
        }
        return phi;
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
        // Pattern 1: return(args) as suffix call
        std::string          callee;
        const pgcodes::GCode *args_code = nullptr;
        if (extractSuffixCall(code, callee, args_code) && callee == "return") {
            return true;
        }
        // Pattern 2: `(` operator with left=return identifier
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "(" &&
            code->getLeft() != nullptr &&
            code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
            code->getLeft()->getValue() == "return") {
            return true;
        }
        return containsReturnPrefix(code->getLeft()) ||
               containsReturnPrefix(code->getRight());
    }

    llvm::Value *emitReturnExpressionValue(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        // Pattern 1: return(args) as suffix call
        std::string          callee;
        const pgcodes::GCode *args_code = nullptr;
        if (extractSuffixCall(code, callee, args_code) && callee == "return") {
            auto args = emitCallArgs(args_code);
            return args.empty() ? llvm::ConstantInt::get(_builder.getInt32Ty(), 0)
                                : args.front();
        }
        // Pattern 2: `(` operator with left=return identifier
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "(" &&
            code->getLeft() != nullptr &&
            code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
            code->getLeft()->getValue() == "return") {
            auto *rhs = code->getRight();
            // void return: right is null or empty GCode
            if (rhs == nullptr ||
                (rhs->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                 rhs->getOper().empty() && rhs->getLeft() == nullptr)) {
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
            }
            return emitExpression(rhs);
        }
        return emitExpression(code);
    }

    llvm::Value *emitExpression(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }

        // Set debug location for expressions with valid source info
        emitDebugLocation(code);

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
            // Struct literal (suffix form): StructName{field: val, ...}
            if (code->getRight() != nullptr &&
                code->getRight()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                code->getRight()->getOper() == "{" &&
                _struct_types.count(code->getValue()) != 0) {
                return emitStructLiteral(code->getValue(),
                                         code->getRight()->getRight());
            }
            return emitValue(code);
        }

        const std::string oper = code->getOper();
        // Empty GCode node (no value, no operator) — treat as 0
        if (oper.empty()) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (oper == ":=" || oper == "=") {
            return emitAssignment(code, oper == ":=");
        }
        if (oper == "match") {
            return emitMatchExpression(code);
        }
        if (oper == "+") {
            auto *left  = emitExpression(code->getLeft());
            auto *right = emitExpression(code->getRight());
            if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
                return emitStrConcat({left, right});
            }
            return _builder.CreateAdd(left, right, "addtmp");
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
        if (oper == "::") {
            return emitEnumVariant(code);
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
            // Field access: expr.field
            return emitFieldAccess(code);
        }
        if (oper == "{") {
            // Struct literal: {left=StructName, right=field:val,...}
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                _struct_types.count(code->getLeft()->getValue()) != 0) {
                return emitStructLiteral(code->getLeft()->getValue(),
                                         code->getRight());
            }
            emitStatement(code);
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (oper == "++" || oper == "--") {
            return emitIncDec(code, oper == "++");
        }
        if (oper == ">>") {
            return emitStreamPush(code);
        }
        if (oper == "<>") {
            return emitInPlaceTransform(code);
        }
        if (oper == "==>") {
            return emitPipelineChain(code);
        }
        {
            auto loc = code->location();
            throw std::runtime_error(
                loc.file + ":" + std::to_string(loc.line) + ":" +
                std::to_string(loc.column) +
                ": unsupported operator in LLVM backend: " + oper);
        }
    }

    llvm::Value *emitReturnExpressionTree(const pgcodes::GCode *code) {
        if (code == nullptr) {
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (code->getValueType() != pgcodes::ValueType::NOT_VALUE) {
            return emitReturnExpressionValue(code);
        }

        const std::string oper = code->getOper();
        if (oper == "::") {
            return emitEnumVariant(code);
        }
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
        if (oper == ".") {
            const auto *left  = code->getLeft();
            const auto *right = code->getRight();

            // If left subtree has no return prefix, handle normally
            if (!containsReturnPrefix(left)) {
                std::string          module_alias, callee;
                const pgcodes::GCode *args_code = nullptr;
                if (extractQualifiedCall(code, module_alias, callee, args_code)) {
                    return emitQualifiedCall(module_alias, callee, args_code);
                }
                return emitFieldAccess(code);
            }

            // Left has return prefix — recurse to strip it and get the value
            auto *base_val = emitReturnExpressionTree(left);

            // Check for qualified call: return(module).func(args)
            if (base_val->getType()->isIntegerTy() ||
                base_val->getType()->isPointerTy()) {
                // Not a struct — might be a qualified call that was
                // already handled inside the recursive call
            }

            // Try extractvalue for struct field access
            if (right &&
                right->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                const std::string &field_name = right->getValue();

                // Check for qualified call pattern where right is a call
                // This handles return(module).func(args) when right has suffix
                // But first try struct field
                auto *struct_type =
                    llvm::dyn_cast<llvm::StructType>(base_val->getType());
                if (struct_type) {
                    const StructInfo *sinfo = nullptr;
                    for (const auto &[sname, si] : _struct_types) {
                        if (si.llvm_type == struct_type) {
                            sinfo = &si;
                            break;
                        }
                    }
                    if (sinfo) {
                        auto fi = sinfo->field_index.find(field_name);
                        if (fi != sinfo->field_index.end()) {
                            return _builder.CreateExtractValue(
                                base_val, fi->second, field_name);
                        }
                        throw std::runtime_error("struct has no field '" +
                                                 field_name + "'");
                    }
                }
                throw std::runtime_error("cannot access field '" +
                                         field_name + "' on non-struct");
            }
            throw std::runtime_error("unsupported return field access");
        }
        if (oper == "(") {
            // Grouping parentheses: left is `)` placeholder
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                code->getLeft()->getOper() == ")") {
                return emitReturnExpressionTree(code->getRight());
            }
            // Pattern 2: `return(expr)` — `(` with left=return
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER &&
                code->getLeft()->getValue() == "return") {
                auto *rhs = code->getRight();
                // void return: right is null or empty GCode
                if (rhs == nullptr ||
                    (rhs->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                     rhs->getOper().empty() && rhs->getLeft() == nullptr)) {
                    return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
                }
                return emitExpression(rhs);
            }
            return emitParenOrCall(code);
        }
        // For any other operator, fall back to the general expression emitter
        return emitExpression(code);
    }

    llvm::Value *emitValue(const pgcodes::GCode *code) {
        switch (code->getValueType()) {
        case pgcodes::ValueType::NUMBER:
            return llvm::ConstantInt::get(_builder.getInt32Ty(),
                                          std::stoi(code->getValue()));
        case pgcodes::ValueType::IDENTIFIER: {
            const auto &name = code->getValue();
            // bool literals
            if (name == "true")
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 1);
            if (name == "false")
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
            // Pipeline control flow constants
            if (name == "CONTINUE")
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
            if (name == "FINISH")
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 1);
            if (name == "TRANSFER_FINISH")
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 2);
            // break/continue are keywords that appear as identifiers
            if (name == "break") {
                if (_loop_stack.empty())
                    throw std::runtime_error("break outside of loop");
                _builder.CreateBr(_loop_stack.back().break_block);
                _terminated = true;
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
            }
            if (name == "continue") {
                if (_loop_stack.empty())
                    throw std::runtime_error("continue outside of loop");
                _builder.CreateBr(_loop_stack.back().continue_block);
                _terminated = true;
                return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
            }
            auto it = _variables.find(name);
            if (it == _variables.end()) {
                throw std::runtime_error("unknown identifier: " + name);
            }
            auto *alloca_type = it->second->getAllocatedType();
            return _builder.CreateLoad(alloca_type, it->second, name);
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

    // ── Struct support ─────────────────────────────────────────────

    // Emit StructName{field: val, field: val, ...}
    llvm::Value *emitEnumVariant(const pgcodes::GCode *code) {
        const auto *left = code->getLeft();
        const auto *right = code->getRight();
        if (left == nullptr || right == nullptr) {
            throw std::runtime_error("invalid enum variant expression");
        }
        const std::string enum_name =
            left->getValueType() != pgcodes::ValueType::NOT_VALUE
                ? left->getValue()
                : "";
        const std::string variant_name =
            right->getValueType() != pgcodes::ValueType::NOT_VALUE
                ? right->getValue()
                : "";
        auto eit = _enum_types.find(enum_name);
        if (eit == _enum_types.end()) {
            throw std::runtime_error("unknown enum type: " + enum_name);
        }
        auto vit = eit->second.variants.find(variant_name);
        if (vit == eit->second.variants.end()) {
            throw std::runtime_error("enum '" + enum_name +
                                     "' has no variant '" + variant_name + "'");
        }
        return llvm::ConstantInt::get(_builder.getInt32Ty(), vit->second);
    }

    llvm::Value *emitStructLiteral(const std::string &struct_name,
                                   const pgcodes::GCode *fields_code) {
        auto it = _struct_types.find(struct_name);
        if (it == _struct_types.end()) {
            throw std::runtime_error("unknown struct type: " + struct_name);
        }
        const auto &info = it->second;
        auto *stype = info.llvm_type;

        // Allocate on stack
        auto *alloca = createVariableSlot(struct_name + ".tmp", stype);

        // Collect field assignments from the comma/colon tree
        std::vector<std::pair<std::string, const pgcodes::GCode *>> assignments;
        collectFieldAssignments(fields_code, assignments);

        // Store each field
        for (const auto &[fname, val_code] : assignments) {
            auto fi = info.field_index.find(fname);
            if (fi == info.field_index.end()) {
                throw std::runtime_error("struct '" + struct_name +
                                         "' has no field '" + fname + "'");
            }
            auto *gep = _builder.CreateStructGEP(stype, alloca, fi->second,
                                                  fname + ".ptr");
            auto *val = emitExpression(val_code);
            _builder.CreateStore(val, gep);
        }

        // Load and return the whole struct value
        return _builder.CreateLoad(stype, alloca, struct_name + ".val");
    }

    void collectFieldAssignments(
        const pgcodes::GCode *code,
        std::vector<std::pair<std::string, const pgcodes::GCode *>> &out) {
        if (code == nullptr) return;
        // Strip grouping
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == "(" && code->getLeft() != nullptr &&
            code->getLeft()->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getLeft()->getOper() == ")") {
            collectFieldAssignments(code->getRight(), out);
            return;
        }
        // Comma-separated
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ",") {
            collectFieldAssignments(code->getLeft(), out);
            collectFieldAssignments(code->getRight(), out);
            return;
        }
        // Single field: field: value
        if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            code->getOper() == ":") {
            if (code->getLeft() != nullptr &&
                code->getLeft()->getValueType() == pgcodes::ValueType::IDENTIFIER) {
                out.emplace_back(code->getLeft()->getValue(), code->getRight());
            }
            return;
        }
    }

    // Resolve a '.' chain into a GEP pointer and the field's LLVM type.
    // Supports nested access: a.b.c
    struct FieldGEP {
        llvm::Value *ptr;
        llvm::Type  *type;
    };

    FieldGEP resolveFieldGEP(const pgcodes::GCode *code) {
        const auto *left  = code->getLeft();
        const auto *right = code->getRight();
        if (right == nullptr ||
            right->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("field name must be an identifier");
        }
        const std::string &field_name = right->getValue();

        llvm::Value *base_ptr  = nullptr;
        llvm::Type  *base_type = nullptr;

        if (left->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            // Simple case: var.field
            auto var_it = _variables.find(left->getValue());
            if (var_it == _variables.end()) {
                throw std::runtime_error("unknown variable: " +
                                         left->getValue());
            }
            base_ptr  = var_it->second;
            base_type = var_it->second->getAllocatedType();
        } else if (left->getValueType() == pgcodes::ValueType::NOT_VALUE &&
                   left->getOper() == ".") {
            // Nested case: expr.inner.field — recurse
            auto inner = resolveFieldGEP(left);
            base_ptr   = inner.ptr;
            base_type  = inner.type;
        } else {
            throw std::runtime_error("unsupported field access expression");
        }

        const StructInfo *sinfo = nullptr;
        for (const auto &[sname, si] : _struct_types) {
            if (si.llvm_type == base_type) {
                sinfo = &si;
                break;
            }
        }
        if (sinfo == nullptr) {
            throw std::runtime_error("not a struct type for field access");
        }

        auto fi = sinfo->field_index.find(field_name);
        if (fi == sinfo->field_index.end()) {
            throw std::runtime_error("struct has no field '" + field_name +
                                     "'");
        }

        auto *gep = _builder.CreateStructGEP(sinfo->llvm_type, base_ptr,
                                              fi->second, field_name + ".ptr");
        auto *ftype = sinfo->llvm_type->getElementType(fi->second);
        return {gep, ftype};
    }

    // Emit expr.field — returns the field value (supports nested access)
    llvm::Value *emitFieldAccess(const pgcodes::GCode *code) {
        auto fg = resolveFieldGEP(code);
        return _builder.CreateLoad(fg.type, fg.ptr,
                                   code->getRight()->getValue());
    }

    llvm::Value *emitComparison(const pgcodes::GCode *code,
                                const std::string   &oper) {
        auto *left  = emitExpression(code->getLeft());
        auto *right = emitExpression(code->getRight());

        // String comparison: use strcmp for == and !=
        if (left->getType()->isPointerTy() && right->getType()->isPointerTy()) {
            auto *cmp_result = _builder.CreateCall(getStrcmp(), {left, right},
                                                   "strcmp");
            llvm::Value *cmp = nullptr;
            if (oper == "==") {
                cmp = _builder.CreateICmpEQ(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "streq");
            } else if (oper == "!=") {
                cmp = _builder.CreateICmpNE(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "strne");
            } else if (oper == "<") {
                cmp = _builder.CreateICmpSLT(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "strlt");
            } else if (oper == ">") {
                cmp = _builder.CreateICmpSGT(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "strgt");
            } else if (oper == "<=") {
                cmp = _builder.CreateICmpSLE(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "strle");
            } else if (oper == ">=") {
                cmp = _builder.CreateICmpSGE(
                    cmp_result,
                    llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "strge");
            } else {
                throw std::runtime_error(
                    "unsupported string comparison operator: " + oper);
            }
            return _builder.CreateZExt(cmp, _builder.getInt32Ty(), "booltmp");
        }

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

        // Handle struct field assignment: expr.field = value
        if (left != nullptr &&
            left->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            left->getOper() == ".") {
            auto *value = emitExpression(code->getRight());
            auto  fg    = resolveFieldGEP(left);
            _builder.CreateStore(value, fg.ptr);
            return value;
        }

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
        // Support prefix (++i) and postfix (i++)
        const auto *operand = code->getRight();
        if (operand == nullptr ||
            operand->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            operand = code->getLeft();
        }
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

    // ── >> stream push: c >> token (append char to string) ──────────
    llvm::Value *emitStreamPush(const pgcodes::GCode *code) {
        auto *lhs = emitExpression(code->getLeft());
        auto *rhs_code = code->getRight();

        // c >> string_var: append char(int) to string
        if (rhs_code != nullptr &&
            rhs_code->getValueType() == pgcodes::ValueType::IDENTIFIER) {
            const std::string &varName = rhs_code->getValue();
            auto it = _variables.find(varName);
            if (it != _variables.end()) {
                auto *rhs = _builder.CreateLoad(it->second->getAllocatedType(),
                                                it->second);
                // If lhs is int and rhs is ptr (string): append char to string
                if (lhs->getType()->isIntegerTy() && rhs->getType()->isPointerTy()) {
                    auto *char_str = emitCharToStr(lhs);
                    auto *result = emitStrConcat({rhs, char_str});
                    _builder.CreateStore(result, it->second);
                    return result;
                }
                // If both are strings: concat
                if (lhs->getType()->isPointerTy() && rhs->getType()->isPointerTy()) {
                    auto *result = emitStrConcat({rhs, lhs});
                    _builder.CreateStore(result, it->second);
                    return result;
                }
            }
        }
        // Fallback: evaluate both sides, return right
        auto *rhs = emitExpression(rhs_code);
        return rhs;
    }

    // ── <> in-place transform: str <> func ──────────────────────────
    // Applies func to each character of string, modifying in-place
    llvm::Value *emitInPlaceTransform(const pgcodes::GCode *code) {
        auto *lhs_code = code->getLeft();
        auto *rhs_code = code->getRight();

        if (lhs_code == nullptr || rhs_code == nullptr) {
            throw std::runtime_error("<> requires left and right operands");
        }

        // Left must be a string variable
        if (lhs_code->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("<> left operand must be a variable");
        }
        const std::string &varName = lhs_code->getValue();
        auto it = _variables.find(varName);
        if (it == _variables.end()) {
            throw std::runtime_error("<> on undefined variable: " + varName);
        }

        auto *str_val = _builder.CreateLoad(it->second->getAllocatedType(),
                                            it->second);
        if (!str_val->getType()->isPointerTy()) {
            throw std::runtime_error("<> left operand must be a string");
        }

        // Right must be a function name
        if (rhs_code->getValueType() != pgcodes::ValueType::IDENTIFIER) {
            throw std::runtime_error("<> right operand must be a function name");
        }
        const std::string &funcName = rhs_code->getValue();
        auto *func = resolveCurrentFunction(funcName);
        if (func == nullptr) {
            throw std::runtime_error("<> function not found: " + funcName);
        }

        // Get string length
        auto *str_len = emitStrLen({str_val});
        // Build new string by applying func to each char
        auto *result_alloca = createVariableSlot("transform.result",
                                                  _builder.getPtrTy());
        auto *empty_str = _builder.CreateGlobalStringPtr("", "empty");
        _builder.CreateStore(empty_str, result_alloca);

        auto *function = _current_function;
        auto *cond_block = llvm::BasicBlock::Create(*_context, "xform.cond", function);
        auto *body_block = llvm::BasicBlock::Create(*_context, "xform.body");
        auto *end_block  = llvm::BasicBlock::Create(*_context, "xform.end");

        auto *idx_alloca = createVariableSlot("xform.idx", _builder.getInt32Ty());
        _builder.CreateStore(llvm::ConstantInt::get(_builder.getInt32Ty(), 0),
                             idx_alloca);
        _builder.CreateBr(cond_block);

        // Condition
        _builder.SetInsertPoint(cond_block);
        auto *idx = _builder.CreateLoad(_builder.getInt32Ty(), idx_alloca);
        auto *cmp = _builder.CreateICmpSLT(idx, str_len, "xform.cmp");
        _builder.CreateCondBr(cmp, body_block, end_block);

        // Body: apply func to char, append to result
        function->insert(function->end(), body_block);
        _builder.SetInsertPoint(body_block);
        auto *ch = emitStrCharAt(str_val, idx);
        auto *transformed = _builder.CreateCall(func, {ch}, "xformed");
        auto *new_char_str = emitCharToStr(transformed);
        auto *cur_result = _builder.CreateLoad(_builder.getPtrTy(), result_alloca);
        auto *new_result = emitStrConcat({cur_result, new_char_str});
        _builder.CreateStore(new_result, result_alloca);
        auto *next_idx = _builder.CreateAdd(
            idx, llvm::ConstantInt::get(_builder.getInt32Ty(), 1));
        _builder.CreateStore(next_idx, idx_alloca);
        _builder.CreateBr(cond_block);

        // End
        function->insert(function->end(), end_block);
        _builder.SetInsertPoint(end_block);
        auto *final_result = _builder.CreateLoad(_builder.getPtrTy(), result_alloca);
        _builder.CreateStore(final_result, it->second);
        return final_result;
    }

    // ── ==> pipeline chain (placeholder) ────────────────────────────
    llvm::Value *emitPipelineChain(const pgcodes::GCode *code) {
        // TODO: Full pipeline chaining implementation
        auto *lhs = emitExpression(code->getLeft());
        auto *rhs = emitExpression(code->getRight());
        return rhs;
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
        // Empty arg from `func()` — parser may emit IDENTIFIER("") or NOT_VALUE with empty oper
        if (current->getValueType() == pgcodes::ValueType::IDENTIFIER &&
            current->getValue().empty()) {
            return;
        }
        if (current->getValueType() == pgcodes::ValueType::NOT_VALUE &&
            current->getOper().empty()) {
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
        if (callee == "panic") {
            auto args = emitCallArgs(args_code);
            auto *msg =
                args.empty()
                    ? _builder.CreateGlobalStringPtr("panic")
                    : args.front();
            auto *panic_fn = _module->getFunction("pg_panic");
            _builder.CreateCall(panic_fn, {msg});
            _builder.CreateUnreachable();
            _terminated = true;
            return msg;
        }
        if (callee == "system") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("system() takes exactly 1 argument");
            auto *sys_fn = _module->getFunction("pg_system");
            return _builder.CreateCall(sys_fn, {args.front()}, "sysret");
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
        // String builtins
        if (callee == "str_concat") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_concat expects 2 arguments");
            return emitStrConcat(args);
        }
        if (callee == "str_len") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("str_len expects 1 argument");
            return emitStrLen(args);
        }
        if (callee == "str_eq") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_eq expects 2 arguments");
            return emitStrEq(args);
        }
        if (callee == "str_substr") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 3)
                throw std::runtime_error("str_substr expects 3 arguments");
            return emitStrSubstr(args);
        }
        if (callee == "int_to_str") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("int_to_str expects 1 argument");
            return emitIntToStr(args);
        }
        if (callee == "str_to_int") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("str_to_int expects 1 argument");
            return emitStrToInt(args);
        }
        if (callee == "str_index_of") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_index_of expects 2 arguments");
            return emitStrIndexOf(args[0], args[1]);
        }
        if (callee == "str_starts_with") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_starts_with expects 2 arguments");
            return emitStrStartsWith(args[0], args[1]);
        }
        if (callee == "str_replace") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 3)
                throw std::runtime_error("str_replace expects 3 arguments");
            return emitStrReplace(args[0], args[1], args[2]);
        }
        // File I/O builtins
        if (callee == "read_file") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("read_file expects 1 argument");
            return emitReadFile(args[0]);
        }
        if (callee == "write_file") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("write_file expects 2 arguments");
            return emitWriteFile(args[0], args[1]);
        }
        // Character builtins
        if (callee == "str_char_at") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_char_at expects 2 arguments");
            return emitStrCharAt(args[0], args[1]);
        }
        if (callee == "char_to_str") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("char_to_str expects 1 argument");
            return emitCharToStr(args[0]);
        }
        // Array builtins
        if (callee == "make_array") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("make_array expects 1 argument (size)");
            return emitMakeArray(args[0]);
        }
        if (callee == "array_get") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("array_get expects 2 arguments");
            return emitArrayGet(args[0], args[1]);
        }
        if (callee == "array_set") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 3)
                throw std::runtime_error("array_set expects 3 arguments");
            return emitArraySet(args[0], args[1], args[2]);
        }
        if (callee == "make_str_array") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("make_str_array expects 1 argument");
            return emitMakeStrArray(args[0]);
        }
        if (callee == "str_array_get") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_array_get expects 2 arguments");
            return emitStrArrayGet(args[0], args[1]);
        }
        if (callee == "str_array_set") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 3)
                throw std::runtime_error("str_array_set expects 3 arguments");
            return emitStrArraySet(args[0], args[1], args[2]);
        }
        if (callee == "args_count") {
            return _builder.CreateLoad(_builder.getInt32Ty(), _argc_global,
                                       "argc");
        }
        if (callee == "args") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("args expects 1 argument");
            auto *argv_ptr =
                _builder.CreateLoad(_builder.getPtrTy(), _argv_global, "argv");
            auto *idx64 =
                _builder.CreateSExt(args[0], _builder.getInt64Ty(), "idx64");
            auto *elem_ptr = _builder.CreateGEP(_builder.getPtrTy(), argv_ptr,
                                                 {idx64}, "arg_ptr");
            return _builder.CreateLoad(_builder.getPtrTy(), elem_ptr, "arg");
        }
        if (callee == "str_ends_with") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("str_ends_with expects 2 arguments");
            auto *fn = _module->getFunction("pg_str_ends_with");
            return _builder.CreateCall(fn, args, "ends_with");
        }
        if (callee == "is_directory") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 1)
                throw std::runtime_error("is_directory expects 1 argument");
            auto *fn = _module->getFunction("pg_is_directory");
            return _builder.CreateCall(fn, args, "is_dir");
        }
        if (callee == "find_pgl_files") {
            auto args = emitCallArgs(args_code);
            if (args.size() != 2)
                throw std::runtime_error("find_pgl_files expects 2 arguments");
            auto *fn = _module->getFunction("pg_find_pgl_files");
            return _builder.CreateCall(fn, args, "n_files");
        }
        // Pipeline builtins
        if (callee == "pipeline_create") {
            auto args = emitCallArgs(args_code);
            auto *elem_size = args.empty()
                ? llvm::ConstantInt::get(_builder.getInt32Ty(), 8)
                : args.front();
            auto *fn = _module->getFunction("pg_pipeline_create");
            return _builder.CreateCall(fn, {elem_size}, "pipe_state");
        }
        if (callee == "pipeline_destroy") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_destroy");
            _builder.CreateCall(fn, {args.front()});
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (callee == "pipeline_cache_append") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_cache_append");
            _builder.CreateCall(fn, {args[0], args[1]});
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (callee == "pipeline_cache_str") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_cache_str");
            return _builder.CreateCall(fn, {args.front()}, "cache_str");
        }
        if (callee == "pipeline_cache_reset") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_cache_reset");
            _builder.CreateCall(fn, {args.front()});
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (callee == "pipeline_emit") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_emit");
            _builder.CreateCall(fn, {args[0], args[1]});
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (callee == "pipeline_output_count") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_output_count");
            return _builder.CreateCall(fn, {args.front()}, "out_count");
        }
        if (callee == "pipeline_output_get") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_output_get");
            return _builder.CreateCall(fn, {args[0], args[1]}, "out_elem");
        }
        if (callee == "pipeline_set_worker") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_set_worker");
            _builder.CreateCall(fn, {args[0], args[1]});
            return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
        }
        if (callee == "pipeline_get_worker") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("pg_pipeline_get_worker");
            return _builder.CreateCall(fn, {args.front()}, "worker_id");
        }
        // Reflection builtins
        if (callee == "reflect_type_count") {
            auto *fn = _module->getFunction("reflect_type_count");
            return _builder.CreateCall(fn, {}, "type_count");
        }
        if (callee == "reflect_type_name") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_type_name");
            return _builder.CreateCall(fn, {args[0]}, "type_name");
        }
        if (callee == "reflect_field_count") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_field_count");
            return _builder.CreateCall(fn, {args[0]}, "field_count");
        }
        if (callee == "reflect_field_name") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_field_name");
            return _builder.CreateCall(fn, {args[0], args[1]}, "field_name");
        }
        if (callee == "reflect_field_type") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_field_type");
            return _builder.CreateCall(fn, {args[0], args[1]}, "field_type");
        }
        if (callee == "reflect_annotation_count") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_annotation_count");
            return _builder.CreateCall(fn, {args[0]}, "ann_count");
        }
        if (callee == "reflect_annotation_key") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_annotation_key");
            return _builder.CreateCall(fn, {args[0], args[1]}, "ann_key");
        }
        if (callee == "reflect_annotation_value") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_annotation_value");
            return _builder.CreateCall(fn, {args[0], args[1]}, "ann_value");
        }
        if (callee == "reflect_annotation_field_index") {
            auto args = emitCallArgs(args_code);
            auto *fn = _module->getFunction("reflect_annotation_field_index");
            return _builder.CreateCall(fn, {args[0], args[1]}, "ann_fidx");
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
        if (callee_function != nullptr) {
            return emitCall(callee_function, module_alias + "." + callee, args_code);
        }
        // Try as a struct method call: StructName.method_name
        std::string mangled = module_alias + "." + callee;
        callee_function = resolveCurrentFunction(mangled);
        if (callee_function != nullptr) {
            return emitCall(callee_function, mangled, args_code);
        }
        throw std::runtime_error("unsupported qualified call: " + mangled);
    }

    llvm::Value *emitParenOrCall(const pgcodes::GCode *code) {
        const auto *left = code->getLeft();
        // Grouping parentheses with no left: just evaluate the right side
        if (left == nullptr) {
            return emitExpression(code->getRight());
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

    llvm::FunctionCallee getSystem() {
        return _module->getOrInsertFunction(
            "system",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy()}, false));
    }

    llvm::FunctionCallee getAbort() {
        return _module->getOrInsertFunction(
            "abort",
            llvm::FunctionType::get(_builder.getVoidTy(), {}, false));
    }

    llvm::FunctionCallee getFprintf() {
        return _module->getOrInsertFunction(
            "fprintf",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    true));
    }

    llvm::FunctionCallee getSignal() {
        auto *handler_ty = llvm::FunctionType::get(_builder.getVoidTy(),
                                                   {_builder.getInt32Ty()}, false);
        auto *handler_ptr_ty = handler_ty->getPointerTo();
        return _module->getOrInsertFunction(
            "signal",
            llvm::FunctionType::get(handler_ptr_ty,
                                    {_builder.getInt32Ty(), handler_ptr_ty}, false));
    }

    llvm::FunctionCallee getBacktrace() {
        return _module->getOrInsertFunction(
            "backtrace",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getInt32Ty()}, false));
    }

    llvm::FunctionCallee getBacktraceSymbols() {
        return _module->getOrInsertFunction(
            "backtrace_symbols",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getInt32Ty()}, false));
    }

    llvm::FunctionCallee getStderr() {
        auto *gv = _module->getOrInsertGlobal("stderr", _builder.getPtrTy());
        (void)gv;
        return getFprintf();
    }

    // Generate pg_print_backtrace helper function
    void emitPrintBacktraceFunc() {
        auto *void_ty = _builder.getVoidTy();
        auto *func_ty = llvm::FunctionType::get(void_ty, {}, false);
        auto *func = llvm::Function::Create(
            func_ty, llvm::Function::InternalLinkage,
            "pg_print_backtrace", _module.get());
        auto *entry = llvm::BasicBlock::Create(*_context, "entry", func);
        _builder.SetInsertPoint(entry);

        // void* buffer[64];
        auto *buf = _builder.CreateAlloca(
            _builder.getPtrTy(),
            llvm::ConstantInt::get(_builder.getInt32Ty(), 64), "bt_buf");
        // int nframes = backtrace(buffer, 64);
        auto *nframes = _builder.CreateCall(
            getBacktrace(), {buf, llvm::ConstantInt::get(_builder.getInt32Ty(), 64)}, "nframes");
        // char** symbols = backtrace_symbols(buffer, nframes);
        auto *symbols = _builder.CreateCall(
            getBacktraceSymbols(), {buf, nframes}, "symbols");

        // Get stderr
        auto *stderr_gv = _module->getOrInsertGlobal("stderr", _builder.getPtrTy());
        auto *stderr_val = _builder.CreateLoad(_builder.getPtrTy(), stderr_gv, "stderr");

        // Print header
        auto *hdr_fmt = _builder.CreateGlobalStringPtr("Stack trace:\n");
        _builder.CreateCall(getFprintf(), {stderr_val, hdr_fmt});

        // Loop: for (int i = 0; i < nframes; i++) fprintf(stderr, "  %s\n", symbols[i]);
        auto *loop_bb = llvm::BasicBlock::Create(*_context, "bt_loop", func);
        auto *end_bb = llvm::BasicBlock::Create(*_context, "bt_end", func);
        _builder.CreateBr(loop_bb);

        _builder.SetInsertPoint(loop_bb);
        auto *i_phi = _builder.CreatePHI(_builder.getInt32Ty(), 2, "i");
        i_phi->addIncoming(llvm::ConstantInt::get(_builder.getInt32Ty(), 0), entry);
        auto *cmp = _builder.CreateICmpSLT(i_phi, nframes, "cmp");
        auto *body_bb = llvm::BasicBlock::Create(*_context, "bt_body", func);
        _builder.CreateCondBr(cmp, body_bb, end_bb);

        _builder.SetInsertPoint(body_bb);
        auto *i64 = _builder.CreateSExt(i_phi, _builder.getInt64Ty());
        auto *sym_ptr = _builder.CreateGEP(_builder.getPtrTy(), symbols, {i64}, "sym_ptr");
        auto *sym = _builder.CreateLoad(_builder.getPtrTy(), sym_ptr, "sym");
        auto *line_fmt = _builder.CreateGlobalStringPtr("  %s\n");
        _builder.CreateCall(getFprintf(), {stderr_val, line_fmt, sym});
        auto *next_i = _builder.CreateAdd(i_phi, llvm::ConstantInt::get(_builder.getInt32Ty(), 1));
        i_phi->addIncoming(next_i, body_bb);
        _builder.CreateBr(loop_bb);

        _builder.SetInsertPoint(end_bb);
        _builder.CreateRetVoid();
    }

    // Generate pg_panic(const char* msg) function
    void emitPanicFunc() {
        auto *void_ty = _builder.getVoidTy();
        auto *func_ty = llvm::FunctionType::get(void_ty, {_builder.getPtrTy()}, false);
        auto *func = llvm::Function::Create(
            func_ty, llvm::Function::ExternalLinkage,
            "pg_panic", _module.get());
        auto *entry = llvm::BasicBlock::Create(*_context, "entry", func);
        _builder.SetInsertPoint(entry);

        auto *msg = func->getArg(0);
        auto *stderr_gv = _module->getOrInsertGlobal("stderr", _builder.getPtrTy());
        auto *stderr_val = _builder.CreateLoad(_builder.getPtrTy(), stderr_gv, "stderr");
        auto *panic_fmt = _builder.CreateGlobalStringPtr("panic: %s\n");
        _builder.CreateCall(getFprintf(), {stderr_val, panic_fmt, msg});

        // Print backtrace
        auto *bt_func = _module->getFunction("pg_print_backtrace");
        if (bt_func) _builder.CreateCall(bt_func, {});

        // Use _exit(1) to avoid triggering SIGABRT handler
        _builder.CreateCall(get_Exit(), {llvm::ConstantInt::get(_builder.getInt32Ty(), 1)});
        _builder.CreateUnreachable();
    }

    // Generate signal handler that prints backtrace on crash
    void emitSignalHandler() {
        auto *void_ty = _builder.getVoidTy();
        auto *handler_ty = llvm::FunctionType::get(void_ty, {_builder.getInt32Ty()}, false);
        auto *handler = llvm::Function::Create(
            handler_ty, llvm::Function::InternalLinkage,
            "pg_signal_handler", _module.get());
        auto *entry = llvm::BasicBlock::Create(*_context, "entry", handler);
        _builder.SetInsertPoint(entry);

        auto *sig = handler->getArg(0);
        auto *stderr_gv = _module->getOrInsertGlobal("stderr", _builder.getPtrTy());
        auto *stderr_val = _builder.CreateLoad(_builder.getPtrTy(), stderr_gv, "stderr");
        auto *fmt = _builder.CreateGlobalStringPtr("\nFatal signal %d received\n");
        _builder.CreateCall(getFprintf(), {stderr_val, fmt, sig});

        auto *bt_func = _module->getFunction("pg_print_backtrace");
        if (bt_func) _builder.CreateCall(bt_func, {});

        _builder.CreateCall(get_Exit(), {llvm::ConstantInt::get(_builder.getInt32Ty(), 1)});
        _builder.CreateUnreachable();
    }

    // _exit() - immediate exit without cleanup (avoids re-triggering signal handlers)
    llvm::FunctionCallee get_Exit() {
        llvm::Type *args[] = {_builder.getInt32Ty()};
        auto *ty = llvm::FunctionType::get(_builder.getVoidTy(), args, false);
        return _module->getOrInsertFunction("_exit", ty);
    }

    llvm::FunctionCallee getPgPanic() {
        auto *func = _module->getFunction("pg_panic");
        if (func) return func;
        // Declare if not yet emitted (will be generated during buildAllFunctions)
        auto *func_ty = llvm::FunctionType::get(_builder.getVoidTy(),
                                                {_builder.getPtrTy()}, false);
        return _module->getOrInsertFunction("pg_panic", func_ty);
    }

    // C runtime helpers for string operations
    llvm::FunctionCallee getStrlen() {
        return _module->getOrInsertFunction(
            "strlen",
            llvm::FunctionType::get(_builder.getInt64Ty(),
                                    {_builder.getPtrTy()}, false));
    }

    llvm::FunctionCallee getStrcmp() {
        return _module->getOrInsertFunction(
            "strcmp",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    false));
    }

    llvm::FunctionCallee getMalloc() {
        return _module->getOrInsertFunction(
            "malloc",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getInt64Ty()}, false));
    }

    llvm::FunctionCallee getMemcpy() {
        return _module->getOrInsertFunction(
            "memcpy",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getPtrTy(),
                                     _builder.getInt64Ty()},
                                    false));
    }

    llvm::FunctionCallee getSnprintf() {
        return _module->getOrInsertFunction(
            "snprintf",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getInt64Ty(),
                                     _builder.getPtrTy()},
                                    true));
    }

    llvm::FunctionCallee getAtoi() {
        return _module->getOrInsertFunction(
            "atoi",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy()}, false));
    }

    llvm::FunctionCallee getFree() {
        return _module->getOrInsertFunction(
            "free",
            llvm::FunctionType::get(_builder.getVoidTy(),
                                    {_builder.getPtrTy()}, false));
    }

    llvm::FunctionCallee getStrstr() {
        return _module->getOrInsertFunction(
            "strstr",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    false));
    }

    llvm::FunctionCallee getStrncmp() {
        return _module->getOrInsertFunction(
            "strncmp",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getPtrTy(),
                                     _builder.getInt64Ty()},
                                    false));
    }

    llvm::FunctionCallee getRealloc() {
        return _module->getOrInsertFunction(
            "realloc",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getInt64Ty()},
                                    false));
    }

    // str_concat(a, b) → new string
    llvm::Value *emitStrConcat(const std::vector<llvm::Value *> &args) {
        auto *a = args[0];
        auto *b = args[1];
        auto *len_a = _builder.CreateCall(getStrlen(), {a}, "len_a");
        auto *len_b = _builder.CreateCall(getStrlen(), {b}, "len_b");
        auto *total = _builder.CreateAdd(len_a, len_b, "total");
        auto *alloc_size = _builder.CreateAdd(
            total, llvm::ConstantInt::get(_builder.getInt64Ty(), 1), "alloc");
        auto *buf = _builder.CreateCall(getMalloc(), {alloc_size}, "buf");
        _builder.CreateCall(getMemcpy(), {buf, a, len_a});
        auto *dst = _builder.CreateGEP(_builder.getInt8Ty(), buf, {len_a},
                                       "dst");
        auto *copy_len = _builder.CreateAdd(
            len_b, llvm::ConstantInt::get(_builder.getInt64Ty(), 1), "cpylen");
        _builder.CreateCall(getMemcpy(), {dst, b, copy_len});
        return buf;
    }

    // str_len(s) → int
    llvm::Value *emitStrLen(const std::vector<llvm::Value *> &args) {
        auto *len64 = _builder.CreateCall(getStrlen(), {args[0]}, "len64");
        return _builder.CreateTrunc(len64, _builder.getInt32Ty(), "len");
    }

    // str_eq(a, b) → int (1 if equal, 0 otherwise)
    llvm::Value *emitStrEq(const std::vector<llvm::Value *> &args) {
        auto *cmp = _builder.CreateCall(getStrcmp(), {args[0], args[1]},
                                        "strcmp");
        auto *eq = _builder.CreateICmpEQ(
            cmp, llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "streq");
        return _builder.CreateZExt(eq, _builder.getInt32Ty(), "streqi");
    }

    // str_substr(s, start, len) → new string
    llvm::Value *emitStrSubstr(const std::vector<llvm::Value *> &args) {
        auto *s     = args[0];
        auto *start = _builder.CreateSExt(args[1], _builder.getInt64Ty(),
                                          "start64");
        auto *len   = _builder.CreateSExt(args[2], _builder.getInt64Ty(),
                                          "len64");
        auto *alloc_size = _builder.CreateAdd(
            len, llvm::ConstantInt::get(_builder.getInt64Ty(), 1), "alloc");
        auto *buf = _builder.CreateCall(getMalloc(), {alloc_size}, "buf");
        auto *src = _builder.CreateGEP(_builder.getInt8Ty(), s, {start},
                                       "src");
        _builder.CreateCall(getMemcpy(), {buf, src, len});
        auto *term = _builder.CreateGEP(_builder.getInt8Ty(), buf, {len},
                                        "term");
        _builder.CreateStore(llvm::ConstantInt::get(_builder.getInt8Ty(), 0),
                             term);
        return buf;
    }

    // int_to_str(n) → new string
    llvm::Value *emitIntToStr(const std::vector<llvm::Value *> &args) {
        auto *buf_size = llvm::ConstantInt::get(_builder.getInt64Ty(), 32);
        auto *buf = _builder.CreateCall(getMalloc(), {buf_size}, "buf");
        auto *fmt = _builder.CreateGlobalStringPtr("%d");
        _builder.CreateCall(getSnprintf(), {buf, buf_size, fmt, args[0]});
        return buf;
    }

    // str_to_int(s) → int
    llvm::Value *emitStrToInt(const std::vector<llvm::Value *> &args) {
        return _builder.CreateCall(getAtoi(), {args[0]}, "atoi");
    }

    // str_index_of(haystack, needle) → int (-1 if not found)
    llvm::Value *emitStrIndexOf(llvm::Value *haystack, llvm::Value *needle) {
        auto *found = _builder.CreateCall(getStrstr(), {haystack, needle}, "found");
        auto *is_null = _builder.CreateICmpEQ(
            found, llvm::ConstantPointerNull::get(_builder.getPtrTy()), "isnull");
        auto *diff = _builder.CreatePtrDiff(_builder.getInt8Ty(), found, haystack, "diff");
        auto *idx32 = _builder.CreateTrunc(diff, _builder.getInt32Ty(), "idx32");
        auto *result = _builder.CreateSelect(
            is_null, llvm::ConstantInt::get(_builder.getInt32Ty(), -1), idx32, "indexOf");
        return result;
    }

    // str_starts_with(s, prefix) → int (1 if yes, 0 if no)
    llvm::Value *emitStrStartsWith(llvm::Value *s, llvm::Value *prefix) {
        auto *prefix_len = _builder.CreateCall(getStrlen(), {prefix}, "plen");
        auto *cmp = _builder.CreateCall(getStrncmp(), {s, prefix, prefix_len}, "cmp");
        auto *eq = _builder.CreateICmpEQ(
            cmp, llvm::ConstantInt::get(_builder.getInt32Ty(), 0), "eq");
        return _builder.CreateZExt(eq, _builder.getInt32Ty(), "starts_with");
    }

    // str_replace(s, old, new) → new string with first occurrence replaced
    llvm::Value *emitStrReplace(llvm::Value *s, llvm::Value *old_str, llvm::Value *new_str) {
        auto *func = _module->getFunction("__pangu_str_replace");
        if (!func) {
            // Build the function inline
            auto *ft = llvm::FunctionType::get(
                _builder.getPtrTy(),
                {_builder.getPtrTy(), _builder.getPtrTy(), _builder.getPtrTy()},
                false);
            func = llvm::Function::Create(ft, llvm::Function::InternalLinkage,
                                          "__pangu_str_replace", _module.get());
            auto *entry = llvm::BasicBlock::Create(*_context, "entry", func);
            auto *found_bb = llvm::BasicBlock::Create(*_context, "found", func);
            auto *notfound_bb = llvm::BasicBlock::Create(*_context, "notfound", func);

            llvm::IRBuilder<> fb(*_context);
            fb.SetInsertPoint(entry);
            auto args_it = func->arg_begin();
            auto *arg_s = &*args_it++;
            auto *arg_old = &*args_it++;
            auto *arg_new = &*args_it++;

            auto *pos = fb.CreateCall(getStrstr(), {arg_s, arg_old}, "pos");
            auto *is_null = fb.CreateICmpEQ(
                pos, llvm::ConstantPointerNull::get(_builder.getPtrTy()), "null");
            fb.CreateCondBr(is_null, notfound_bb, found_bb);

            // Not found: return copy of original
            fb.SetInsertPoint(notfound_bb);
            auto *slen = fb.CreateCall(getStrlen(), {arg_s}, "slen");
            auto *salloc = fb.CreateAdd(slen, llvm::ConstantInt::get(fb.getInt64Ty(), 1));
            auto *scopy = fb.CreateCall(getMalloc(), {salloc}, "scopy");
            fb.CreateCall(getMemcpy(), {scopy, arg_s, salloc});
            fb.CreateRet(scopy);

            // Found: build prefix + new + suffix
            fb.SetInsertPoint(found_bb);
            auto *prefix_len = fb.CreatePtrDiff(fb.getInt8Ty(), pos, arg_s, "plen");
            auto *old_len = fb.CreateCall(getStrlen(), {arg_old}, "olen");
            auto *new_len = fb.CreateCall(getStrlen(), {arg_new}, "nlen");
            auto *s_len = fb.CreateCall(getStrlen(), {arg_s}, "slen2");
            auto *result_len = fb.CreateSub(s_len, old_len);
            result_len = fb.CreateAdd(result_len, new_len);
            auto *buf_size = fb.CreateAdd(result_len, llvm::ConstantInt::get(fb.getInt64Ty(), 1));
            auto *buf = fb.CreateCall(getMalloc(), {buf_size}, "buf");
            // copy prefix
            fb.CreateCall(getMemcpy(), {buf, arg_s, prefix_len});
            // copy new
            auto *dst1 = fb.CreateGEP(fb.getInt8Ty(), buf, {prefix_len}, "dst1");
            fb.CreateCall(getMemcpy(), {dst1, arg_new, new_len});
            // copy suffix
            auto *dst2 = fb.CreateGEP(fb.getInt8Ty(), buf, {fb.CreateAdd(prefix_len, new_len)}, "dst2");
            auto *suffix_start = fb.CreateGEP(fb.getInt8Ty(), pos, {old_len}, "suf");
            auto *suffix_len = fb.CreateSub(s_len, fb.CreateAdd(prefix_len, old_len));
            auto *suffix_copy = fb.CreateAdd(suffix_len, llvm::ConstantInt::get(fb.getInt64Ty(), 1));
            fb.CreateCall(getMemcpy(), {dst2, suffix_start, suffix_copy});
            fb.CreateRet(buf);
        }
        return _builder.CreateCall(func, {s, old_str, new_str}, "replaced");
    }

    // read_file(path) → string (reads entire file into malloc'd buffer)
    llvm::Value *emitReadFile(llvm::Value *path) {
        auto fopen_fn = _module->getOrInsertFunction(
            "fopen",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    false));
        auto fseek_fn = _module->getOrInsertFunction(
            "fseek",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getInt64Ty(),
                                     _builder.getInt32Ty()},
                                    false));
        auto ftell_fn = _module->getOrInsertFunction(
            "ftell",
            llvm::FunctionType::get(_builder.getInt64Ty(),
                                    {_builder.getPtrTy()}, false));
        auto fread_fn = _module->getOrInsertFunction(
            "fread",
            llvm::FunctionType::get(_builder.getInt64Ty(),
                                    {_builder.getPtrTy(), _builder.getInt64Ty(),
                                     _builder.getInt64Ty(), _builder.getPtrTy()},
                                    false));
        auto fclose_fn = _module->getOrInsertFunction(
            "fclose",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy()}, false));

        auto *mode = _builder.CreateGlobalStringPtr("rb");
        auto *fp = _builder.CreateCall(fopen_fn, {path, mode}, "fp");

        // fseek(fp, 0, SEEK_END=2)
        _builder.CreateCall(fseek_fn,
                            {fp, llvm::ConstantInt::get(_builder.getInt64Ty(), 0),
                             llvm::ConstantInt::get(_builder.getInt32Ty(), 2)});
        auto *size = _builder.CreateCall(ftell_fn, {fp}, "fsize");

        // fseek(fp, 0, SEEK_SET=0)
        _builder.CreateCall(fseek_fn,
                            {fp, llvm::ConstantInt::get(_builder.getInt64Ty(), 0),
                             llvm::ConstantInt::get(_builder.getInt32Ty(), 0)});

        auto *alloc = _builder.CreateAdd(
            size, llvm::ConstantInt::get(_builder.getInt64Ty(), 1), "alloc");
        auto *buf = _builder.CreateCall(getMalloc(), {alloc}, "buf");

        _builder.CreateCall(fread_fn,
                            {buf, llvm::ConstantInt::get(_builder.getInt64Ty(), 1),
                             size, fp});
        _builder.CreateCall(fclose_fn, {fp});

        // null-terminate
        auto *term = _builder.CreateGEP(_builder.getInt8Ty(), buf, {size},
                                        "term");
        _builder.CreateStore(llvm::ConstantInt::get(_builder.getInt8Ty(), 0),
                             term);
        return buf;
    }

    // write_file(path, content) → int (0 on success)
    llvm::Value *emitWriteFile(llvm::Value *path, llvm::Value *content) {
        auto fopen_fn = _module->getOrInsertFunction(
            "fopen",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    false));
        auto fputs_fn = _module->getOrInsertFunction(
            "fputs",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy(), _builder.getPtrTy()},
                                    false));
        auto fclose_fn = _module->getOrInsertFunction(
            "fclose",
            llvm::FunctionType::get(_builder.getInt32Ty(),
                                    {_builder.getPtrTy()}, false));

        auto *mode = _builder.CreateGlobalStringPtr("w");
        auto *fp = _builder.CreateCall(fopen_fn, {path, mode}, "fp");
        _builder.CreateCall(fputs_fn, {content, fp});
        _builder.CreateCall(fclose_fn, {fp});
        return llvm::ConstantInt::get(_builder.getInt32Ty(), 0);
    }

    // str_char_at(s, i) → int (character code at index)
    llvm::Value *emitStrCharAt(llvm::Value *s, llvm::Value *idx) {
        auto *idx64 = _builder.CreateSExt(idx, _builder.getInt64Ty(), "idx64");
        auto *ptr = _builder.CreateGEP(_builder.getInt8Ty(), s, {idx64},
                                       "charptr");
        auto *ch = _builder.CreateLoad(_builder.getInt8Ty(), ptr, "ch");
        return _builder.CreateZExt(ch, _builder.getInt32Ty(), "char_i32");
    }

    // char_to_str(c) → string (single character string)
    llvm::Value *emitCharToStr(llvm::Value *ch) {
        auto *buf = _builder.CreateCall(
            getMalloc(),
            {llvm::ConstantInt::get(_builder.getInt64Ty(), 2)}, "buf");
        auto *ch8 = _builder.CreateTrunc(ch, _builder.getInt8Ty(), "ch8");
        _builder.CreateStore(ch8, buf);
        auto *term = _builder.CreateGEP(
            _builder.getInt8Ty(), buf,
            {llvm::ConstantInt::get(_builder.getInt64Ty(), 1)}, "term");
        _builder.CreateStore(llvm::ConstantInt::get(_builder.getInt8Ty(), 0),
                             term);
        return buf;
    }

    // make_array(size) → ptr to int32 array (calloc'd, zero-initialized)
    llvm::Value *emitMakeArray(llvm::Value *size) {
        auto calloc_fn = _module->getOrInsertFunction(
            "calloc",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getInt64Ty(), _builder.getInt64Ty()},
                                    false));
        auto *size64 = _builder.CreateSExt(size, _builder.getInt64Ty(), "sz64");
        auto *elem_size = llvm::ConstantInt::get(_builder.getInt64Ty(), 4);
        return _builder.CreateCall(calloc_fn, {size64, elem_size}, "arr");
    }

    // array_get(arr, i) → int32
    llvm::Value *emitArrayGet(llvm::Value *arr, llvm::Value *idx) {
        auto *idx64 = _builder.CreateSExt(idx, _builder.getInt64Ty(), "idx64");
        auto *ptr = _builder.CreateGEP(_builder.getInt32Ty(), arr, {idx64},
                                       "elem_ptr");
        return _builder.CreateLoad(_builder.getInt32Ty(), ptr, "elem");
    }

    // array_set(arr, i, val) → val
    llvm::Value *emitArraySet(llvm::Value *arr, llvm::Value *idx,
                              llvm::Value *val) {
        auto *idx64 = _builder.CreateSExt(idx, _builder.getInt64Ty(), "idx64");
        auto *ptr = _builder.CreateGEP(_builder.getInt32Ty(), arr, {idx64},
                                       "elem_ptr");
        _builder.CreateStore(val, ptr);
        return val;
    }

    // make_str_array(size) → ptr to string (ptr) array
    llvm::Value *emitMakeStrArray(llvm::Value *size) {
        auto calloc_fn = _module->getOrInsertFunction(
            "calloc",
            llvm::FunctionType::get(_builder.getPtrTy(),
                                    {_builder.getInt64Ty(), _builder.getInt64Ty()},
                                    false));
        auto *size64 = _builder.CreateSExt(size, _builder.getInt64Ty(), "sz64");
        auto *elem_size = llvm::ConstantInt::get(_builder.getInt64Ty(), 8);
        return _builder.CreateCall(calloc_fn, {size64, elem_size}, "sarr");
    }

    // str_array_get(arr, i) → string (ptr)
    llvm::Value *emitStrArrayGet(llvm::Value *arr, llvm::Value *idx) {
        auto *idx64 = _builder.CreateSExt(idx, _builder.getInt64Ty(), "idx64");
        auto *ptr = _builder.CreateGEP(_builder.getPtrTy(), arr, {idx64},
                                       "selem_ptr");
        return _builder.CreateLoad(_builder.getPtrTy(), ptr, "selem");
    }

    // str_array_set(arr, i, val) → val
    llvm::Value *emitStrArraySet(llvm::Value *arr, llvm::Value *idx,
                                 llvm::Value *val) {
        auto *idx64 = _builder.CreateSExt(idx, _builder.getInt64Ty(), "idx64");
        auto *ptr = _builder.CreateGEP(_builder.getPtrTy(), arr, {idx64},
                                       "selem_ptr");
        _builder.CreateStore(val, ptr);
        return val;
    }

  private:
    std::unique_ptr<llvm::LLVMContext> _context;
    std::unique_ptr<llvm::Module>      _module;
    llvm::IRBuilder<>                  _builder;
    const Program                     &_program;
    std::string                        _source_path;
    llvm::Function                    *_current_function = nullptr;
    llvm::FunctionCallee               _printf;
    llvm::FunctionCallee               _exit;
    std::map<std::string, llvm::Function *>    _declared_functions;
    std::map<std::string, llvm::AllocaInst *>  _variables;
    std::map<std::string, StructInfo>          _struct_types;
    std::map<std::string, EnumInfo>            _enum_types;
    std::map<std::string, PipelineInfo>        _pipeline_types;
    std::vector<LoopContext>                    _loop_stack;
    std::string                                _current_module_id;
    const std::map<std::string, std::string>  *_current_imports = nullptr;
    bool                                      _terminated = false;
    llvm::GlobalVariable                      *_argc_global = nullptr;
    llvm::GlobalVariable                      *_argv_global = nullptr;
    // DWARF debug info
    std::unique_ptr<llvm::DIBuilder>           _di_builder;
    llvm::DICompileUnit                       *_di_cu = nullptr;
    llvm::DIFile                              *_di_file = nullptr;
    llvm::DIScope                             *_current_di_scope = nullptr;
    std::map<std::string, llvm::DIFile *>      _di_files;
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
                    std::string &error,
                    int argc, const char *argv[]) {
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

    // Register JIT function addresses for source-level backtraces
    auto debug_reg = jit->lookup("__pangu_register_jit_debug");
    if (debug_reg) {
        auto *reg_fn = debug_reg->toPtr<void (*)()>();
        reg_fn();
    }

    using MainFunc = int (*)(int, const char **);
    auto *entry = symbol_or_err->toPtr<MainFunc>();
    exit_code   = entry(argc, argv);
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

    // Look for pangu_builtins.c relative to the executable or source tree
    std::string runtime_path;
    const char *candidates[] = {
        "runtime/pangu_builtins.c",
        "../runtime/pangu_builtins.c",         // when running from build/
    };
    for (auto *cand : candidates) {
        if (std::ifstream(cand).good()) {
            runtime_path = cand;
            break;
        }
    }

    std::string command = "clang -g -Wno-override-module -rdynamic -x ir " +
                                quotePath(ir_path);
    if (!runtime_path.empty()) {
        command += " -x c " + quotePath(runtime_path);
    }
    command += " -o " + quotePath(output_path);
    const int rc = std::system(command.c_str());

    // Clean up intermediate IR file
    std::remove(ir_path.c_str());

    if (rc != 0) {
        error = "clang failed when compiling llvm ir";
        return false;
    }
    return true;
}

} // namespace llvm_backend
} // namespace pangu
