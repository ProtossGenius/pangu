#include "pgcodes/switchers.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/enums.h"
#include "pipeline/pipeline.h"
#include <stdexcept>
#include <string>
namespace pangu {
namespace pgcodes {
void CodesSwitcher::onFail(const std::string &errMsg) {
    throw std::runtime_error("codes switcher: " + errMsg);
}
using namespace std;
void CodesSwitcher::onChoice() {
    lexer::DLex *lex = get(0);
    if (lexer::isSpace(lex) || lexer::isComments(lex) || lexer::isEof(lex)) {
        return _factory->choicePipeline(ECodeType::Ignore);
    }

    if (lexer::makeSymbol("{") == *lex || lexer::makeSymbol("(") == *lex ||
        lexer::makeSymbol("[") == *lex) {
        return _factory->choicePipeline(ECodeType::Block);
    }

    if (lexer::makeIdentifier("if") == *lex) {
        return _factory->choicePipeline(ECodeType::If);
    }

    if (lexer::makeIdentifier("do") == *lex) {
        return _factory->choicePipeline(ECodeType::Do);
    }

    if (lexer::makeIdentifier("for") == *lex) {
        return _factory->choicePipeline(ECodeType::For);
    }

    if (lexer::makeIdentifier("while") == *lex) {
        return _factory->choicePipeline(ECodeType::While);
    }

    if (lexer ::makeIdentifier("goto") == *lex) {
        return _factory->choicePipeline(ECodeType::Goto);
    }

    if (lexer::makeIdentifier("switch") == *lex) {
        return _factory->choicePipeline(ECodeType::Switch);
    }

    _factory->choicePipeline(ECodeType::Normal);
}

} // namespace pgcodes
} // namespace pangu
