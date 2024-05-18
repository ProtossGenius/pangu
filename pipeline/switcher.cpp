#include "pipeline/switcher.h"
#include "pipeline/pipeline.h"
#include <vector>
namespace pglang {
void ISwitcher::accept(PData &&data) {
    if (_current_pipeline) {
        _current_pipeline->accept(std::move(data));
        return;
    }

    _cached_datas.emplace_back(std::move(data));
    auto choisedPipeline = onChoice();
    if (choisedPipeline) {
        _current_pipeline = choisedPipeline;
        while (true) {
            size_t beforeSize = _cached_datas.size();
            dealCachedDatas();
            if (beforeSize == _cached_datas.size()) {
                break;
            }
        }
        return;
    }
}

void ISwitcher::dealCachedDatas() {
    std::vector<PData> cachedDatas;
    cachedDatas.swap(_cached_datas);
    for (PData &data : cachedDatas) {
        accept(std::move(data));
    }
}
} // namespace pglang