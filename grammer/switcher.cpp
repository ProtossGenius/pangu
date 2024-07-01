#include "grammer/switcher.h"
#include "grammer/enums.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <sstream>
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    auto first = get(0);
    if (lexer::makeIdentifier("package") == first) {
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

    _factory->onFail("can't choice pipeline: " + ss.str());
}
} // namespace grammer
} // namespace pangu