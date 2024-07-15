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
    if (lexer::isSpace(lex) || lexer::isComments(lex)) {
        return _factory->choicePipeline(ECodeType::Ignore);
    }

    if (lexer::makeSymbol("{") == *lex) {
        return _factory->choicePipeline(ECodeType::Block);
    }

    _factory->choicePipeline(ECodeType::Normal);
}

} // namespace pgcodes
} // namespace pangu
