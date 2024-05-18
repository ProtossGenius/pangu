#include "pipeline/pipeline.h"
#include "pipeline/parts_dealer.h"
#include "pipeline/product_pack.h"
#include "pipeline/switcher.h"
#include <utility>
namespace pglang {
void IPipelineFactory::init(PSwitcher             &&switcher,
                            std::vector<PPipeline> &_pipelines,
                            PPartsDealer          &&partsDealer,
                            PProductPack          &&finalProductPacker) {
    _switcher = std::move(switcher);
    _pipelines.swap(_pipelines);
    _parts_dealer         = std::move(partsDealer);
    _final_product_packer = std::move(finalProductPacker);
}

void IPipelineFactory::pushProduct(PProduct &&pro, IProductPack *pack) {
    _product_stack.emplace(std::move(pro));
    _packer_stack.emplace(pack);
}
void IPipelineFactory::accept(PData &&data) {
    _switcher->accept(std::move(data));
}
void IPipelineFactory::pushProduct(PProduct &&pro) {
    pushProduct(std::move(pro), _final_product_packer.get());
}
IPipelineFactory::~IPipelineFactory() {}
void IPipelineFactory::packProduct() {
    PProduct &&pro = std::move(_product_stack.top());
    _product_stack.pop();
    IProductPack *pack = _packer_stack.top();
    _packer_stack.pop();
    pack->accept(std::move(pro));
}

void IPipeline::pushProduct(PProduct &&pro, IProductPack *pack) {
    _factory->pushProduct(std::move(pro), pack);
}

void IPipeline::undealData(PData &&data) {
    getSwitcher()->pushToCache(std::move(data));
}
} // namespace pglang
