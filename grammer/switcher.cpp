#include "grammer/switcher.h"
#include "grammer/enums.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <sstream>
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    lexer::DLex &first = get(0);
    lexer::DLex  pack  = lexer::makeIdentifier("package");
    if (pack == first) {
        _factory->choicePipeline(EGrammer::Package);
        return;
    }
    if (lexer::makeIdentifier("import") == first) {
        _factory->choicePipeline(EGrammer::Import);
        return;
    }

    std::cout << "==========================" << first.to_string() << std::endl;
    if (lexer::makeIdentifier("type") == first) {
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                  << first.to_string() << std::endl;
        if (first.typeId() == lexer::ELexPipeline::Comments ||
            first.typeId() == lexer::ELexPipeline::Space) {
            return;
        }
        if (_cached_datas.size() < 3) {
            return;
        }
        auto third = get(2);
        if (lexer::makeIdentifier("struct") == third) {
            _factory->choicePipeline(EGrammer::Struct);
            return;
        }

        _factory->onFail("no such type: " + third.get() +
                         "cached data = " + getCached());
    }
    if (first.typeId() == lexer::ELexPipeline::Space ||
        first.typeId() == lexer::ELexPipeline::Comments) {
        _factory->choicePipeline(EGrammer::Ignore);
        return;
    }
    _factory->onFail("GrammerSwitcher can't choice pipeline: cached data = " +
                     getCached());
}
} // namespace grammer
} // namespace pangu