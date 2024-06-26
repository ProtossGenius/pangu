#pragma once
#include "pipeline/pipeline.h"
#include <string>
namespace pangu {
namespace lexer {

class DInChar : public pglang::IData {
  public:
    DInChar(char c)
        : _c(c) {}
    DInChar(char c, int type)
        : _c(c)
        , _type(type) {}
    int  typeId() override { return _type; }
    char get() { return _c; }

  private:
    char _c;
    int  _type;
};

class DLex : public pglang::IData {
  public:
    DLex(int tid)
        : _typeId(tid) {}
    int          typeId() override { return _typeId; }
    std::string &get() { return _lex; }

  private:
    std::string _lex;
    int         _typeId;
};

} // namespace lexer
} // namespace pangu