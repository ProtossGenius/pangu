#include "pipeline/pipeline.h"
// #include "pipeline/parts_dealer.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
#include <iostream>
#include <utility>
namespace pglang {
IPipelineFactory::IPipelineFactory(PSwitcher               &&switcher,
                                   std::map<int, PPipeline> &_pipelines,
                                   //  PPartsDealer          &&partsDealer,
                                   ProductPack finalProductPacker)
    : _switcher(std::move(switcher))
    , _pipelines(_pipelines)
    , _final_product_packer(finalProductPacker) {
    _switcher->_factory = this;
}
IPipeline *IPipelineFactory::getPipeline() {
    return _index_stack.empty() || _index_stack.size() < _packer_stack.size()
               ? nullptr
               : _pipelines[ _index_stack.top() ].get();
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
IPipelineFactory::~IPipelineFactory() {
    if (!_index_stack.empty()) {
        std::cerr << "error: IPipielineFactory _index_stack not empty."
                  << std::endl;
    }
}
void IPipelineFactory::packProduct() {
    PProduct pro = std::move(_product_stack.top());
    _product_stack.pop();
    ProductPack pack = _packer_stack.top();
    _packer_stack.pop();
    pack(this, std::move(pro));
    _switcher->afterPack();
    _index_stack.pop();
}

void IPipelineFactory::onFail(const std::string &errMsg) {
    _switcher->onFail(errMsg);
}

} // namespace pglang
