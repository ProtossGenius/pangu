#include "grammer/enums.h"
#include "grammer/switcher.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <sstream>
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    auto first = get(0);
    if ("package" == first.get()) {
        _factory->choicePipeline(EGrammer::Package);
        return;
    }
    if ("import" == first.get()) {
        _factory->choicePipeline(EGrammer::Import);
        return;
    }
    if ("type" == first.get()) {
        if (_cached_datas.size() < 3) {
            return;
        }
        auto third = get(3).get();
        if ("struct" == third) {
            _factory->choicePipeline(EGrammer::Struct);
            return;
        }
        _factory->onFail("no such type: " + third);
    }
    std::stringstream ss;
    for (size_t i = 0; i < _cached_datas.size(); ++i) {
        ss << get(i).get() << " ";
    }

    _factory->onFail("can't choice pipeline: " + ss.str());
}
} // namespace grammer
} // namespace pangu