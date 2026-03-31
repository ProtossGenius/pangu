#pragma once
#include "pipeline/datas.h"
#include <cstddef>
#include <set>
#include <string>
namespace pangu {
namespace lexer {

struct SourceLocation {
    std::string file;
    int         line   = 1;
    int         column = 1;
    std::string line_text;

    bool valid() const { return !file.empty(); }
};

std::string stripInternalErrorPrefix(const std::string &message);
std::string formatDiagnostic(const SourceLocation &location,
                             const std::string   &message,
                             size_t               highlight_width = 1);

class DInChar : public pglang::IData {
  public:
    DInChar(char c)
        : _c(c)
        , _type(0) {}
    DInChar(char c, int type)
        : _c(c)
        , _type(type) {}
    DInChar(char c, int type, const SourceLocation &location)
        : _c(c)
        , _type(type)
        , _location(location) {}
    int         typeId() const override { return _type; }
    std::string to_string() override { return std::string("char: ") + _c; }
    char        get() { return _c; }
    const SourceLocation &location() const { return _location; }

  private:
    char           _c;
    int            _type;
    SourceLocation _location;
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
    void         setLocationIfUnset(const SourceLocation &location) {
        if (!_location.valid()) {
            _location = location;
        }
    }
    const SourceLocation &location() const { return _location; }
    size_t                highlightWidth() const {
        return _lex.empty() ? 1 : _lex.size();
    }
    bool         operator==(const DLex &lex) const {
        return _typeId == lex._typeId && _lex == lex._lex;
    }

    bool operator!=(const DLex &lex) { return !(*this == lex); }
    ~DLex() {}

  private:
    int            _typeId;
    std::string    _lex;
    SourceLocation _location;
};

int                                symbol_power(const std::string &s);
extern const std::set<std::string> symbols;
extern const std::set<std::string> keywords;
bool is_keywords(DLex *lex);
} // namespace lexer
} // namespace pangu
