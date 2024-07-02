#pragma once
#include "pipeline/declare.h"
#include "pipeline/parts_dealer.h"
#include <functional>
#include <map>
#include <stack>
#include <string>
namespace pglang {
class IData {
  public:
    virtual int         typeId() const = 0;
    virtual std::string to_string() { return ""; }
    virtual ~IData() {}
};

class IProduct : public IData {
  public:
    virtual int         typeId() const override = 0;
    virtual std::string to_string() override { return ""; }
    virtual ~IProduct() {}
};
// dataflow:
// (IData)  --> ISwitcher --> IPipeline --> (IParts)
//          --> IPartsDealer  ++> IProduct .
class IPipelineFactory {
    // init area.
  public:
    IPipelineFactory(
        const std::string &name, PSwitcher &&switcher,
        std::map<int, PPipeline>                    &_pipelines,
        /*PPartsDealer &&partsDealer, */ ProductPack finalProductPacker);
    ISwitcher    *getSwitcher() { return _switcher.get(); }
    IPipeline    *getPipeline();
    void          choicePipeline(size_t index) { _index_stack.push(index); }
    IPartsDealer *getPartsDealer() { return _parts_dealer.get(); }
    void          pushProduct(PProduct &&pro, ProductPack pack);
    void          pushProduct(PProduct &&pro);
    IProduct     *getTopProduct() {
        return _product_stack.empty() ? nullptr : _product_stack.top().get();
    }
    void               packProduct();
    void               onFail(const std::string &errMsg);
    void               undealData(PData &&data);
    const std::string &name() { return _name; }

  public:
    void accept(PData &&data);
    virtual ~IPipelineFactory();

  protected:
    PSwitcher                 _switcher;
    std::map<int, PPipeline> &_pipelines;
    PPartsDealer              _parts_dealer;
    std::stack<PProduct>      _product_stack;
    std::stack<ProductPack>   _packer_stack;
    ProductPack               _final_product_packer;
    std::stack<size_t>        _index_stack;
    std::string               _name;
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