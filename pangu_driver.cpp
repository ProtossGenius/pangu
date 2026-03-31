#include "pangu_driver.h"

#include "grammer/grammer.h"
#include "lexer/lexer.h"
#include "llvm_backend/llvm_backend.h"

#include <fstream>
#include <iostream>
#include <lexer/pipelines.h>
#include <string>

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

int runParsePipeline(const std::string &input_path) {
    auto grm = grammer::create(grammer::PACK_PRINT);
    lexer::analysis(input_path.c_str(), lexer::packNext(grm.get()));
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

int emitIR(const std::string &input_path) {
    if (!ensureReadable(input_path)) {
        return -1;
    }
    std::cout << llvm_backend::emitModuleIR(input_path);
    return 0;
}

int reportPendingMode(Mode mode) {
    const char *mode_name = "";
    switch (mode) {
    case Mode::EMIT_IR: mode_name = "emit-ir"; break;
    case Mode::COMPILE: mode_name = "compile"; break;
    case Mode::RUN: mode_name = "run"; break;
    case Mode::PARSE: mode_name = "parse"; break;
    }
    std::cerr << "mode '" << mode_name
              << "' is planned but not implemented yet. Use 'parse' for the "
                 "current frontend pipeline."
              << std::endl;
    return 2;
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
    case Mode::COMPILE:
    case Mode::RUN: return reportPendingMode(options.mode);
    }
    printUsage(std::cerr, argv[ 0 ]);
    return -1;
}

} // namespace driver
} // namespace pangu
