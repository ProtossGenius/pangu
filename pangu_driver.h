#pragma once

#include <string>

namespace pangu {
namespace driver {

enum class Mode { PARSE = 0, EMIT_IR, COMPILE, RUN };

struct Options {
    Mode        mode        = Mode::PARSE;
    std::string input_path;
};

int run(int argc, const char *argv[]);

} // namespace driver
} // namespace pangu
