#include "pipeline/switcher.h"
#include "pipeline/pipeline.h"
#include <iostream>
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
    // std::cout << "before on choice, pipeline:"
    //           << (choisedPipeline ? typeid(choisedPipeline).name() : "null")
    //           << std::endl;
    if (choisedPipeline == nullptr) {
        onChoice();
        choisedPipeline = _factory->getPipeline();
    }

    // std::cout << "after on choice, pipeline:"
    //           << (choisedPipeline ? typeid(choisedPipeline).name() : "null")
    //           << std::endl;

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
            accept(std::move(data));
        }
        if (beforeSize == cachedDatas.size()) {
            break;
        }
    }
}
} // namespace pglang