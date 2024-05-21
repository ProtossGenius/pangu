#pragma once
#include "pipeline/declare.h"
#include "pipeline/parts_dealer.h"
#include <functional>
#include <iostream>
#include <memory>
#include <stack>
#include <vector>
namespace pglang {
class IData {
  public:
    virtual int typeId() = 0;
    virtual ~IData() {}
};

class IParts : virtual public IData {
  public:
    virtual ~IParts() {}
};
class IProduct : virtual public IData {
  public:
    virtual ~IProduct() {}
};
// dataflow:
// (IData)  --> ISwitcher --> IPipeline --> (IParts)
//          --> IPartsDealer  ++> IProduct .
class IPipelineFactory {
    // init area.
  public:
    IPipelineFactory(
        PSwitcher &&switcher, std::vector<PPipeline> &_pipelines,
        /*PPartsDealer &&partsDealer, */ ProductPack finalProductPacker);
    ISwitcher *getSwitcher() { return _switcher.get(); }
    IPipeline *getPipeline(size_t index) { return _pipelines[ index ].get(); }
    IPartsDealer *getPartsDealer() { return _parts_dealer.get(); }
    void          pushProduct(PProduct &&pro, ProductPack pack);
    void          pushProduct(PProduct &&pro);
    IProduct     *getTopProduct() { return _product_stack.top().get(); }
    void          packProduct();
    void          onFail(const std::string &errMsg);
    void          undealData(PData &&data);

  public:
    void accept(PData &&data);
    virtual ~IPipelineFactory();

  protected:
    PSwitcher               _switcher;
    std::vector<PPipeline> &_pipelines;
    PPartsDealer            _parts_dealer;
    std::stack<PProduct>    _product_stack;
    std::stack<ProductPack> _packer_stack;
    ProductPack             _final_product_packer;
};

class IPipeline {
  public:
    virtual void accept(IPipelineFactory *factory, PData &&data) = 0;
    // switch to this pipeline will call onSwitch.
    virtual void onSwitch(IPipelineFactory *) = 0;
    virtual ~IPipeline() {}
};

class Reg {
  public:
    Reg(std::function<void()> action) { action(); }
};

} // namespace pglang