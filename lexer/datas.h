#pragma once
#include "pipeline/datas.h"
#include <set>
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
    int         typeId() const override { return _type; }
    std::string to_string() override { return std::string("char: ") + _c; }
    char        get() { return _c; }

  private:
    char _c;
    int  _type;
};

class DLex : public pglang::IProduct {
  public:
    DLex(int tid)
        : _typeId(tid) {}
    DLex(int tid, const std::string &lex)
        : _typeId(tid)
        , _lex(lex) {}
    int          typeId() const override { return _typeId; }
    std::string &get() { return _lex; }
    std::string  to_string() override;
    bool         operator==(const DLex &lex) const {
        return _typeId == lex._typeId && _lex == lex._lex;
    }

    bool operator!=(const DLex &lex) { return !(*this == lex); }
    ~DLex() {}

  private:
    int         _typeId;
    std::string _lex;
};

int                                symbol_power(const std::string &s);
extern const std::set<std::string> symbols;
extern const std::set<std::string> keywords;
bool is_keywords(DLex *lex);
} // namespace lexer
} // namespace pangu