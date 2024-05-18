#pragma once
#include "pipeline/pipeline.h"
#include <string>
namespace pangu {
namespace lexer {
enum DataType { DT_INPUT = 0, DT_LEX = 1 };

class DInChar : pglang::IData {
  public:
    DInChar(char c)
        : _c(c) {}
    int  typeId() override { return DataType::DT_INPUT; }
    char get() { return _c; }

  private:
    char _c;
};

class DLex : pglang::IData {
  public:
    DLex(std::string lex)
        : _lex(lex) {}
    int         typeId() override { return DataType::DT_LEX; }
    std::string get() { return _lex; }

  private:
    std::string _lex;
};

} // namespace lexer
} // namespace pangu