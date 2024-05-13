#include "pipeline/pipeline.h"
#include "pipeline/parts_dealer.h"
#include "pipeline/switcher.h"

namespace pglang {
void IPipelineFactory::init(PSwitcher             &&switcher,
                            std::vector<PPipeline> &_pipelines,
                            PPartsDealer          &&partsDealer) {
    _switcher = std::move(switcher);
    _pipelines.swap(_pipelines);
    _parts_dealer = std::move(partsDealer);
}

} // namespace pglang
