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
    const char   c   = ((DInChar *) data.get())->get();                        \
    DLex        *lex = (DLex *) factory->getTopProduct();                      \
    std::string &str = lex->get()

bool isNumber(const std::string &str, std::string &errMsg) { return true; }
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

const std::set<char> getSymbolCharSet() {
    std::string    str = "!@$%^&*()_+-={}|[]\\:;<>?,./~";
    std::set<char> s;
    for (const char c : str) {
        s.insert(c);
    }
    return s;
}

void PipeSymbol::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    const static std::set<char> symbols = getSymbolCharSet();
    if (symbols.count(c) == 0) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    str.push_back(c);
}

void PipeComments::accept(IPipelineFactory *factory, PData &&data) {
    GET_CHAR(data);
    if (str.size() >= 2) {
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
    if ('\n' == c && str[ str.size() - 1 ] != '\\') {
        factory->packProduct();
        return;
    }
    str.push_back(c);
}
} // namespace lexer
} // namespace pangu