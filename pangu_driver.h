#pragma once

#include <string>

namespace pangu {
namespace driver {

enum class Mode { PARSE = 0, EMIT_IR, COMPILE, RUN };

struct Options {
    Mode        mode         = Mode::PARSE;
    std::string input_path;
    bool        show_help    = false;
    bool        show_version = false;
};

int run(int argc, const char *argv[]);

} // namespace driver
} // namespace pangu
