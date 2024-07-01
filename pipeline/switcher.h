#pragma once

#include "pipeline/declare.h"
#include <vector>
namespace pglang {
class ISwitcher {
    friend class IPipelineFactory;

  private:
    virtual void onChoice() = 0;
    virtual void readForAnalysis(const PData &data) {}
    void         dealCachedDatas();
    void         afterPack() { _current_pipeline = nullptr; }

  public:
    void accept(PData &&data);
    virtual ~ISwitcher() {}

    void pushToCache(PData &&data) {
        _cached_datas.emplace_back(std::move(data));
    }

  protected:
    virtual void onFail(const std::string &errMsg) = 0;

  protected:
    IPipeline         *_current_pipeline;
    IPipelineFactory  *_factory;
    std::vector<PData> _cached_datas;
};
} // namespace pglang
