#include "pipeline/switcher.h"
#include "pipeline/pipeline.h"
#include <vector>
namespace pglang {
void ISwitcher::accept(PData &&data) {
    readForAnalysis(data);
    if (_current_pipeline) {
        _current_pipeline->accept(_factory, std::move(data));
        dealCachedDatas();
        return;
    }
    _cached_datas.emplace_back(std::move(data));
    onChoice();
    auto choisedPipeline = _factory->getPipeline();
    if (nullptr == choisedPipeline) {
        return;
    }
    _current_pipeline = choisedPipeline;
    _current_pipeline->onSwitch(_factory);
    dealCachedDatas();
    return;
}

void ISwitcher::dealCachedDatas() {
    while (true) {
        std::vector<PData> cachedDatas;
        cachedDatas.swap(_cached_datas);
        auto beforeSize = cachedDatas.size();
        for (PData &data : cachedDatas) {
            accept(std::move(data));
        }
        if (beforeSize == cachedDatas.size()) {
            break;
        }
    }
}
} // namespace pglang