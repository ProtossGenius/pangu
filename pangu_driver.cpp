#include "pangu_driver.h"

#include "grammer/datas.h"
#include "grammer/grammer.h"
#include "lexer/lexer.h"
#include "llvm_backend/llvm_backend.h"

#include <fstream>
#include <iostream>
#include <lexer/pipelines.h>
#include <memory>
#include <string>
#include <cstdlib>

namespace pangu {
namespace driver {
namespace {

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
    return argc == 3;
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
        std::cerr << "parse failed: " << error << std::endl;
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

int emitIR(const std::string &input_path) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    std::string parse_error;
    auto package = parsePackage(input_path, parse_error);
    if (package == nullptr) {
        std::cerr << "parse failed: " << parse_error << std::endl;
        return -1;
    }
    std::string ir_text;
    std::string error;
    if (!llvm_backend::emitPackageIR(*package, input_path, ir_text, error)) {
        std::cerr << "emit ir failed: " << error << std::endl;
        return -1;
    }
    std::cout << ir_text;
    return 0;
}

int runDirect(const std::string &input_path) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    std::string parse_error;
    auto package = parsePackage(input_path, parse_error);
    if (package == nullptr) {
        std::cerr << "parse failed: " << parse_error << std::endl;
        return -1;
    }
    int         exit_code = 0;
    std::string error;
    if (!llvm_backend::runPackageMain(*package, input_path, exit_code, error)) {
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
    std::string parse_error;
    auto package = parsePackage(input_path, parse_error);
    if (package == nullptr) {
        std::cerr << "parse failed: " << parse_error << std::endl;
        return -1;
    }
    const std::string output_path = "build/" + moduleNameFromPath(input_path);
    std::string       error;
    if (!llvm_backend::compilePackageToExecutable(*package, input_path,
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
    case Mode::RUN: return runDirect(options.input_path);
    }
    printUsage(std::cerr, argv[ 0 ]);
    return -1;
}

} // namespace driver
} // namespace pangu
