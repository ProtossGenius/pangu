#include "lexer/datas.h"
#include "pipeline/declare.h"
#include <cassert>
#include <cctype>
#include <lexer/pipelines.h>
#include <set>
namespace pangu {
namespace lexer {
#define GET_CHAR(data)                                                         \
    const char   c     = ((DInChar *) data.get())->get();                      \
    const char   cType = ((DInChar *) data.get())->typeId();                   \
    DLex        *lex   = (DLex *) factory->getTopProduct();                    \
    std::string &str   = lex->get()
std::string DLex::to_string() {
    return "lexer <" + LEX_PIPE_ENUM[ _typeId ] + "> content = \"" + _lex +
           "\"";
}
bool isNumber(const std::string &str, std::string &errMsg) {
    if (str.size() == 1) {
        return true;
    }

    if (str[ 0 ] == '0') {                        // 8 or 16 or 0.x
        if ('x' == str[ 1 ] || 'X' == str[ 1 ]) { // 16
            for (size_t i = 2; i < str.size(); ++i) {
                if (str[ i ] == '_') continue;
                if (isdigit(str[ i ]) || (str[ i ] >= 'a' && str[ i ] <= 'f') ||
                    (str[ i ] >= 'A' && str[ i ] <= 'F')) {
                    continue;
                }
                errMsg = "not a 16x number, number is " + str;
                return false;
            }

            return true;
        }
        if (isdigit(str[ 1 ])) { // 8
            for (size_t i = 2; i < str.size(); ++i) {
                if (str[ i ] == '_') continue;
                if (str[ i ] >= '0' && str[ i ] <= 7) {
                    continue;
                }
                errMsg = "not a 8x number, number is " + str;
                return false;
            }
            return true;
        }

        if (str[ 1 ] == '.') {
            for (size_t i = 2; i < str.size(); ++i) {
                if (isdigit(str[ i ])) {
                    continue;
                }

                errMsg = "not a float, number is " + str;
                return false;
            }
            return true;
        }
    }

    int pointCnt = 0;
    for (size_t i = 1; i < str.size(); ++i) {
        char c = str[ i ];
        if (isdigit(c) || c == '_') {
            continue;
        }
        if (c == '.') ++pointCnt;
        if (pointCnt > 1) {
            errMsg = "more than one point in float, number is " + str;
        }
    }

    return true;
}
void PipeNumber::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (isdigit(c) || isalpha(c) || '.' == c || '_' == c) {
        str.push_back(c);
        return;
    }
    std::string errMsg;
    if (!isNumber(str, errMsg)) {
        factory->onFail(errMsg);
    }

    factory->packProduct();
    factory->undealData(std::move(data));
}

void PipeIdentifier::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);

    if (isdigit(c) || isalpha(c) || '_' == c || '$' == c) {
        str.push_back(c);
        return;
    }
    std::string errMsg;
    if (!isNumber(str, errMsg)) {
        factory->onFail(errMsg);
    }

    factory->packProduct();
    factory->undealData(std::move(data));
}

void PipeSpace::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    str.push_back(c);
    factory->packProduct();
}

void PipeSymbol::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    std::string add = str;
    add.push_back(c);
    if (symbols.count(add) != 0) {
        str.push_back(c);
        return;
    }
    if (str.size() == 0) {
        factory->onFail("not a symbole: " + add);
    }

    factory->undealData(std::move(data));
    factory->packProduct();
}

void PipeComments::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (cType == -1) { // EOF
        factory->undealData(std::move(data));
        if ('*' == str[ 1 ]) {
            factory->onFail("multi-line comment not end with '*/'");
        }
        if ('/' == str[ 1 ]) {
            factory->packProduct();
            return;
        }
    }
    // for comments start with '//'
    if ('/' == str[ 1 ] && '\n' == c) {
        factory->packProduct();
        return;
    }
    if ('/' == c && '*' == str[ str.size() - 1 ] && '*' == str[ 1 ]) {
        str.push_back(c);
        factory->packProduct();
        return;
    }
    str.push_back(c);
}
const static std::map<char, std::string> CAST_ESCAPE({
    {'a', "\a"},
    {'b', "\b"},
    {'t', "\t"},
    {'n', "\n"},
    {'v', "\v"},
    {'f', "\f"},
    {'r', "\r"},
    {'e', "\e"},
    {'?', "\?"},
    {'\'', "\'"},
    {'"', "\""},
    {'\\', "\\"},
});

int getFlashPos(const std::string &str) {
    auto len   = str.size();
    auto flash = '\\';
    for (int i = 1; i < 4; ++i) {
        if (len < i) {
            break;
        }
        if (str[ len - i ] == flash) {
            return i;
        }
    }
    return -1;
}
// @return if push lastChar to str;
bool tryEscape(std::string &str, int flashPos, char lastChar) {
    assert(flashPos > 1);
    auto isO8  = [](char c) { return c >= '0' && c < '8'; };
    auto isO16 = [](char c) {
        return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    auto escape = [](std::string &str, int flashPos, int o) {
        int result = 0;
        for (int i = flashPos - 1; i > 0; --i) {
            char c      = str[ str.size() - i ];
            int  intVal = isdigit(c)               ? c - '0'
                          : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                                                   : c - 'A' + 10;
            result *= o;
            result += intVal;
        }
        while (flashPos--) {
            str.pop_back();
        }
        str.push_back((char(result)));
    };

    std::function<bool(char)> escapeCharCheck = isO8;
    int                       o               = 8;
    if (str[ str.size() - flashPos + 1 ] == 'x') { // 16
        escapeCharCheck = isO16;
        o               = 16;
    }
    if (escapeCharCheck(lastChar)) {
        str.push_back(lastChar);
        if (flashPos == 3) {
            escape(str, flashPos + 1, o);
        }
        return true;
    }
    escape(str, flashPos, o);

    return false;
}
void PipeString::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (str.size() > 1) {
        auto len = str.size();
        if (str[ len - 1 ] == '\\') {
            if (CAST_ESCAPE.count(c)) {
                str.pop_back();
                str += CAST_ESCAPE.at(c);
                return;
            }
            str.push_back(c);
            return;
        }
        auto flashPos = getFlashPos(str);
        if (flashPos != -1 && tryEscape(str, flashPos, c)) {
            return;
        }
        if (str[ 0 ] == c) {
            str = str.substr(1);
            factory->packProduct();
            return;
        }
    }
    str.push_back(c);
}
void PipeMacro::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (-1 == cType) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    if ('\n' == c && str[ str.size() - 1 ] != '\\') {
        factory->packProduct();
        return;
    }
    lex->to_string();
    str.push_back(c);
}
void PipeEof::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    str.push_back(c);
    factory->packProduct();
}

} // namespace lexer
} // namespace pangu