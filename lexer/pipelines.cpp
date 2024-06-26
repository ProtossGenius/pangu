#include "lexer/datas.h"
#include "pipeline/declare.h"
#include <cctype>
#include <iostream>
#include <lexer/pipelines.h>
#include <memory>
#include <set>
#include <stdexcept>
namespace pangu {
namespace lexer {
#define GET_CHAR(data)                                                         \
    const char   c     = ((DInChar *) data.get())->get();                      \
    const char   cType = ((DInChar *) data.get())->typeId();                   \
    DLex        *lex   = (DLex *) factory->getTopProduct();                    \
    std::string &str   = lex->get()

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
    const static std::set<std::string> symbols{
        "!",  "!=",  "@", "%", "%=", "^",  "^=", "&",  "&=", "&&", "&&=",
        "*",  "*=",  "(", ")", "-",  "-=", "+",  "+=", "=",  "==", "|",
        "||", "||=", "[", "]", "{",  "}",  "<-", "<",  "<=", ">",  "->",
        ">=", ",",   ".", "?", "/",  "/=", "\\", ";",  ":",  "::"};
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

void PipeString::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (str.size() > 1) {
        if (str[ str.size() - 1 ] != '\\' && str[ 0 ] == c) {
            str.push_back(c);
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
    str.push_back(c);
}
void PipeEof::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    str.push_back(c);
    factory->packProduct();
}

} // namespace lexer
} // namespace pangu