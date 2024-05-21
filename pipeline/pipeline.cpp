#include "pipeline/pipeline.h"
// #include "pipeline/parts_dealer.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
#include <utility>
namespace pglang {
IPipelineFactory::IPipelineFactory(PSwitcher             &&switcher,
                                   std::vector<PPipeline> &_pipelines,
                                   //  PPartsDealer          &&partsDealer,
                                   ProductPack finalProductPacker)
    : _switcher(std::move(switcher))
    , _pipelines(_pipelines)
    , _final_product_packer(finalProductPacker) {
    _switcher->_factory = this;
}

void IPipelineFactory::pushProduct(PProduct &&pro, ProductPack pack) {
    _product_stack.emplace(std::move(pro));
    _packer_stack.push(pack);
}
void IPipelineFactory::accept(PData &&data) {
    _switcher->accept(std::move(data));
}
void IPipelineFactory::pushProduct(PProduct &&pro) {
    pushProduct(std::move(pro), _final_product_packer);
}

void IPipelineFactory::undealData(PData &&data) {
    _switcher->pushToCache(std::move(data));
}
IPipelineFactory::~IPipelineFactory() {}
void IPipelineFactory::packProduct() {
    PProduct pro = std::move(_product_stack.top());
    _product_stack.pop();
    ProductPack pack = _packer_stack.top();
    _packer_stack.pop();
    pack(this, std::move(pro));
    _switcher->afterPack();
}

void IPipelineFactory::onFail(const std::string &errMsg) {
    _switcher->onFail(errMsg);
}

} // namespace pglang
