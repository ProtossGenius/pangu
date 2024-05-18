#pragma once

#include "pipeline/declare.h"
#include <vector>
namespace pglang {
class ISwitcher {
  private:
    virtual IPipeline *onChoice(const PData &data) = 0;
    void               dealCachedDatas();

  public:
    void accept(PData &&data);
    virtual ~ISwitcher(){};

    void pushToCache(PData &&data) {
        _cached_datas.emplace_back(std::move(data));
    }

  protected:
  protected:
    IPipeline         *_current_pipeline;
    IPipelineFactory  *_factory;
    std::vector<PData> _cached_datas;
};
} // namespace pglang
