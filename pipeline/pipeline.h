#pragma once
#include "pipeline/declare.h"
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
    void       init(PSwitcher &&switcher, std::vector<PPipeline> &_pipelines,
                    PPartsDealer &&partsDealer, PProductPack &&finalProductPacker);
    ISwitcher *getSwitcher() { return _switcher.get(); }
    IPipeline *getPipeline(size_t index) { return _pipelines[ index ].get(); }
    IPartsDealer *getPartsDealer() { return _parts_dealer.get(); }
    void          pushProduct(PProduct &&pro, IProductPack *pack);
    void          pushProduct(PProduct &&pro);
    PProduct     &getTopProduct() { return _product_stack.top(); }
    void          packProduct();

  public:
    void accept(PData &&data);
    virtual ~IPipelineFactory();

  protected:
    PSwitcher                  _switcher;
    std::vector<PPipeline>     _pipelines;
    PPartsDealer               _parts_dealer;
    std::stack<PProduct>       _product_stack;
    std::stack<IProductPack *> _packer_stack;
    PProductPack               _final_product_packer;
};

class IPipeline {
  public:
    virtual void accept(PData &&data) = 0;
    virtual ~IPipeline() {}

  protected:
    IPartsDealer *getPartsDealer() { return _factory->getPartsDealer(); }
    ISwitcher    *getSwitcher() { return _factory->getSwitcher(); }

  protected:
    void pushProduct(PProduct &&pro, IProductPack *pack);
    void undealData(PData &&data);

  protected:
    IPipelineFactory *_factory;
};
} // namespace pglang