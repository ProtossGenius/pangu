#include "grammer/switcher.h"
#include "grammer/enums.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <sstream>
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    auto &first = get(0);
    auto  pack  = lexer::makeIdentifier("package");
    if (pack == first) {
        _factory->choicePipeline(EGrammer::Package);
        return;
    }
    if (lexer::makeIdentifier("import") == first) {
        _factory->choicePipeline(EGrammer::Import);
        return;
    }
    if (lexer::makeIdentifier("type") == first) {
        if (_cached_datas.size() < 3) {
            return;
        }
        auto third = get(3);
        if (lexer::makeIdentifier("struct") == third) {
            _factory->choicePipeline(EGrammer::Struct);
            return;
        }
        _factory->onFail("no such type: " + third.get());
    }
    std::stringstream ss;
    for (size_t i = 0; i < _cached_datas.size(); ++i) {
        ss << get(i).get() << " ";
    }
    if (first.typeId() == lexer::ELexPipeline::Space ||
        first.typeId() == lexer::ELexPipeline::Comments) {
        _factory->choicePipeline(EGrammer::Ignore);
        return;
    }
    _factory->onFail("GrammerSwitcher can't choice pipeline: " + ss.str());
}
} // namespace grammer
} // namespace pangu