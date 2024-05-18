#include "lexer/switchers.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/pipeline.h"
#include <ctype.h>
namespace pangu {
namespace lexer {

IPipeline *LexSwitcher::onChoice() {
    char c = get(0).get();
    if (isdigit(c)) {
        return _factory->getPipeline(ELexPipeline::Number);
    }
    if (isalpha(c) || '_' == c) {
        return _factory->getPipeline(ELexPipeline::Identifier);
    }

    if (isspace(c)) {
        return _factory->getPipeline(ELexPipeline::Space);
    }

    if ('\'' == c || '"' == c || '`' == c) {

        return _factory->getPipeline(ELexPipeline::String);
    }

    if ('/' == c) {
        if (_cached_datas.size() == 1) {
            return nullptr;
        }
        char c1 = get(1).get();
        if ('/' == c1 || '*' == c1) {
            return _factory->getPipeline(ELexPipeline::Comments);
        }
    }
    return _factory->getPipeline(ELexPipeline::Symbol);
}

} // namespace lexer
} // namespace pangu
