#include "pangu_driver.h"

#include "grammer/datas.h"
#include "grammer/grammer.h"
#include "lexer/lexer.h"
#include "llvm_backend/llvm_backend.h"
#include "sema/sema.h"

#include <fstream>
#include <iostream>
#include <lexer/pipelines.h>
#include <memory>
#include <map>
#include <sstream>
#include <string>
#include <cstdlib>
#include <limits.h>
#include <unistd.h>
#include <vector>

namespace pangu {
namespace driver {
namespace {

struct LoadedPackage {
    std::string                            module_id;
    std::string                            source_path;
    std::unique_ptr<grammer::GPackage>     package;
    std::map<std::string, std::string>     import_alias_to_module;
};

void printUsage(std::ostream &os, const char *argv0) {
    os << "usage: " << argv0 << " [parse|emit-ir|compile|run] <file.pgl>\n";
}

bool parseMode(const std::string &mode_name, Mode &mode) {
    if (mode_name == "parse") {
        mode = Mode::PARSE;
        return true;
    }
    if (mode_name == "emit-ir") {
        mode = Mode::EMIT_IR;
        return true;
    }
    if (mode_name == "compile") {
        mode = Mode::COMPILE;
        return true;
    }
    if (mode_name == "run") {
        mode = Mode::RUN;
        return true;
    }
    return false;
}

bool parseArgs(int argc, const char *argv[], Options &options) {
    if (argc <= 1) {
        return false;
    }
    if (argc == 2) {
        options.input_path = argv[ 1 ];
        return true;
    }

    const std::string mode_name = argv[ 1 ];
    if (!parseMode(mode_name, options.mode)) {
        return false;
    }
    options.input_path = argv[ 2 ];
    return argc >= 3;
}

bool tryParseWithPrinter(const std::string &input_path, std::string &error) {
    try {
        auto grm = grammer::create(grammer::PACK_PRINT);
        lexer::analysis(input_path.c_str(), lexer::packNext(grm.get()));
        return true;
    } catch (const std::exception &ex) {
        error = ex.what();
        return false;
    }
}

int runParsePipeline(const std::string &input_path) {
    std::string error;
    if (!tryParseWithPrinter(input_path, error)) {
        std::cerr << error << std::endl;
        return -1;
    }
    return 0;
}

bool ensureReadable(const std::string &input_path) {
    std::ifstream input(input_path);
    if (input.good()) {
        return true;
    }
    std::cerr << "can not read input file: " << input_path << std::endl;
    return false;
}

std::string moduleNameFromPath(const std::string &input_path) {
    const std::string::size_type slash_pos = input_path.find_last_of("/\\");
    const std::string            file_name =
        slash_pos == std::string::npos ? input_path
                                       : input_path.substr(slash_pos + 1);
    const std::string::size_type dot_pos = file_name.find_last_of('.');
    return dot_pos == std::string::npos ? file_name
                                        : file_name.substr(0, dot_pos);
}

bool ensureBuildDir() {
    return std::system("mkdir -p build") == 0;
}

std::string canonicalPath(const std::string &path) {
    char resolved[ PATH_MAX ];
    if (realpath(path.c_str(), resolved) == nullptr) {
        return "";
    }
    return resolved;
}

std::string directoryName(const std::string &path) {
    const std::string::size_type slash_pos = path.find_last_of("/\\");
    return slash_pos == std::string::npos ? "." : path.substr(0, slash_pos);
}

std::string ensurePglSuffix(const std::string &path) {
    return path.size() >= 4 && path.substr(path.size() - 4) == ".pgl"
               ? path
               : path + ".pgl";
}

std::string findImportPath(const std::string &from_source,
                           const std::string &import_path) {
    std::vector<std::string> candidates;
    if (import_path.rfind("stdlib/", 0) == 0) {
        candidates.push_back(ensurePglSuffix(import_path));
    } else if (!import_path.empty() && import_path[ 0 ] == '/') {
        candidates.push_back(ensurePglSuffix(import_path));
    } else {
        candidates.push_back(
            ensurePglSuffix(directoryName(from_source) + "/" + import_path));
        candidates.push_back(ensurePglSuffix(import_path));
    }

    for (const auto &candidate : candidates) {
        const std::string resolved = canonicalPath(candidate);
        if (!resolved.empty()) {
            return resolved;
        }
    }
    return "";
}

std::string moduleIdFromSourcePath(const std::string &source_path) {
    return source_path;
}

std::unique_ptr<grammer::GPackage> parsePackage(const std::string &input_path,
                                                std::string       &error) {
    std::unique_ptr<grammer::GPackage> package;
    try {
        auto packer = [&](auto, auto pro) {
            package.reset(static_cast<grammer::GPackage *>(pro.release()));
        };
        auto grm = grammer::create(packer);
        lexer::analysis(input_path.c_str(), lexer::packNext(grm.get()));
    } catch (const std::exception &ex) {
        error = ex.what();
        return nullptr;
    }
    return package;
}

bool loadPackageRecursive(
    const std::string                         &source_path,
    std::map<std::string, LoadedPackage>      &loaded_packages,
    std::vector<std::string>                  &load_order,
    std::string                               &error) {
    const std::string canonical_source = canonicalPath(source_path);
    if (canonical_source.empty()) {
        error = "can not read input file: " + source_path;
        return false;
    }
    if (loaded_packages.count(canonical_source) != 0) {
        return true;
    }

    std::string parse_error;
    auto package = parsePackage(canonical_source, parse_error);
    if (package == nullptr) {
        error = parse_error;
        return false;
    }

    LoadedPackage loaded;
    loaded.module_id   = moduleIdFromSourcePath(canonical_source);
    loaded.source_path = canonical_source;
    loaded.package     = std::move(package);

    for (const auto &it : loaded.package->imports()) {
        const auto &imp = it.second;
        const std::string import_source =
            findImportPath(canonical_source, imp->getPackage());
        if (import_source.empty()) {
            error = lexer::formatDiagnostic(
                imp->location(),
                "can not resolve import '" + imp->getPackage() + "'");
            return false;
        }
        if (!loadPackageRecursive(import_source, loaded_packages, load_order,
                                  error)) {
            return false;
        }
        loaded.import_alias_to_module[ imp->alias() ] =
            moduleIdFromSourcePath(import_source);
    }

    load_order.push_back(canonical_source);
    loaded_packages[ canonical_source ] = std::move(loaded);
    return true;
}

bool buildProgram(const std::string &entry_path, llvm_backend::Program &program,
                  std::string &error) {
    std::map<std::string, LoadedPackage> loaded_packages;
    std::vector<std::string>             load_order;
    if (!loadPackageRecursive(entry_path, loaded_packages, load_order, error)) {
        return false;
    }

    const std::string canonical_entry = canonicalPath(entry_path);
    program.entry_module_id = moduleIdFromSourcePath(canonical_entry);
    for (const auto &source_path : load_order) {
        const auto &loaded = loaded_packages.at(source_path);
        llvm_backend::PackageUnit unit;
        unit.module_id              = loaded.module_id;
        unit.source_path            = loaded.source_path;
        unit.package                = loaded.package.get();
        unit.import_alias_to_module = loaded.import_alias_to_module;
        program.packages.push_back(unit);
    }

    static std::vector<std::unique_ptr<grammer::GPackage>> storage;
    storage.clear();
    for (auto &it : loaded_packages) {
        storage.push_back(std::move(it.second.package));
    }
    return true;
}

bool runSemaChecks(const llvm_backend::Program &program) {
    auto result = sema::checkProgram(program);
    if (!result.ok) {
        for (const auto &diag : result.errors) {
            std::cerr << diag.message << std::endl;
        }
        return false;
    }
    return true;
}

int emitIR(const std::string &input_path) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    llvm_backend::Program program;
    std::string           error;
    if (!buildProgram(input_path, program, error)) {
        std::cerr << error << std::endl;
        return -1;
    }
    if (!runSemaChecks(program)) {
        return -1;
    }
    std::string ir_text;
    if (!llvm_backend::emitProgramIR(program, input_path, ir_text, error)) {
        std::cerr << "emit ir failed: " << error << std::endl;
        return -1;
    }
    std::cout << ir_text;
    return 0;
}

int runDirect(const std::string &input_path, int pgl_argc,
              const char *pgl_argv[]) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    llvm_backend::Program program;
    std::string           error;
    if (!buildProgram(input_path, program, error)) {
        std::cerr << error << std::endl;
        return -1;
    }
    if (!runSemaChecks(program)) {
        return -1;
    }
    int         exit_code = 0;
    if (!llvm_backend::runProgramMain(program, input_path, exit_code, error,
                                      pgl_argc, pgl_argv)) {
        std::cerr << "run failed: " << error << std::endl;
        return -1;
    }
    return exit_code;
}

int compileSource(const std::string &input_path) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    if (!ensureBuildDir()) {
        std::cerr << "can not create build directory" << std::endl;
        return -1;
    }
    llvm_backend::Program program;
    std::string           error;
    if (!buildProgram(input_path, program, error)) {
        std::cerr << error << std::endl;
        return -1;
    }
    if (!runSemaChecks(program)) {
        return -1;
    }
    const std::string output_path = "build/" + moduleNameFromPath(input_path);
    if (!llvm_backend::compileProgramToExecutable(program, input_path,
                                                  output_path, error)) {
        std::cerr << "compile failed: " << error << std::endl;
        return -1;
    }
    std::cout << output_path << std::endl;
    return 0;
}

} // namespace

int run(int argc, const char *argv[]) {
    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(std::cerr, argv[ 0 ]);
        return -1;
    }

    switch (options.mode) {
    case Mode::PARSE: return runParsePipeline(options.input_path);
    case Mode::EMIT_IR: return emitIR(options.input_path);
    case Mode::COMPILE: return compileSource(options.input_path);
    case Mode::RUN:
        // Pass remaining args (from argv[2]) to the PGL program
        return runDirect(options.input_path, argc - 2, argv + 2);
    }
    printUsage(std::cerr, argv[ 0 ]);
    return -1;
}

} // namespace driver
} // namespace pangu
