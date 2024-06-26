#include "lexer/switchers.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/pipeline.h"
#include <ctype.h>
#include <stdexcept>
#include <string>
namespace pangu {
namespace lexer {
void LexSwitcher::onFail(const std::string &errMsg) {
    throw std::runtime_error("file:" + std::to_string(lineNo) + "," +
                             std::to_string(linePos) + " " + errMsg);
}
using namespace std;
void LexSwitcher::onChoice() {
    if (-1 == get(0).typeId()) {
        return _factory->choicePipeline(ELexPipeline::Eof);
    }
    char c = get(0).get();
    if ('#' == c) {
        return _factory->choicePipeline(ELexPipeline::Macro);
    }
    if (isdigit(c)) {
        return _factory->choicePipeline(ELexPipeline::Number);
    }
    if (isalpha(c) || '_' == c) {
        return _factory->choicePipeline(ELexPipeline::Identifier);
    }

    if (isspace(c)) {
        return _factory->choicePipeline(ELexPipeline::Space);
    }

    if ('\'' == c || '"' == c || '`' == c) {

        return _factory->choicePipeline(ELexPipeline::String);
    }

    if ('/' == c) {
        if (_cached_datas.size() == 1) {
            return;
        }
        char c1 = get(1).get();
        if ('/' == c1 || '*' == c1) {
            return _factory->choicePipeline(ELexPipeline::Comments);
        }
    }
    return _factory->choicePipeline(ELexPipeline::Symbol);
}

} // namespace lexer
} // namespace pangu
