#include "pipeline/switcher.h"
#include "pipeline/pipeline.h"
#include <vector>
namespace pglang {
void ISwitcher::accept(PData &&data) {
    readForAnalysis(data);
    auto choisedPipeline = _factory->getPipeline();
    if (choisedPipeline && !_factory->needSwitch()) {
        choisedPipeline->accept(_factory, std::move(data));
        dealCachedDatas();
        return;
    }
    _cached_datas.emplace_back(std::move(data));
    if (choisedPipeline == nullptr) {
        onChoice();
        choisedPipeline = _factory->getPipeline();
    }

    if (nullptr == choisedPipeline) {
        return;
    }
    choisedPipeline->onSwitch(_factory);
    dealCachedDatas();
    return;
}

void ISwitcher::dealCachedDatas() {
    while (true) {
        std::vector<PData> cachedDatas;
        cachedDatas.swap(_cached_datas);
        auto beforeSize = cachedDatas.size();
        for (PData &data : cachedDatas) {
            if (_cached_datas.empty()) {
                accept(std::move(data));
            } else {
                _cached_datas.emplace_back(std::move(data));
            }
        }
        if (beforeSize == _cached_datas.size()) {
            break;
        }
    }
}
} // namespace pglang