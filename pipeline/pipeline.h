#pragma once
#include "pipeline/declare.h"
#include <memory>
#include <stack>
#include <vector>
namespace pglang {
// dataflow:
// (IData)  --> ISwitcher --> IPipeline --> (IParts)
//          --> IPartsDealer  ++> IProduct .
class IPipelineFactory {
    // init area.
  public:
    void init(PSwitcher &&switcher, std::vector<PPipeline> &_pipelines,
              PPartsDealer &&partsDealer);

  public:
    virtual void read(PData data) {}

  protected:
    PSwitcher                _switcher;
    std::vector<PPipeline>   _pipelines;
    PPartsDealer             _parts_dealer;
    std::stack<PProduct>     _product_stack;
    std::stack<PProductPack> _packer_stack;
};
} // namespace pglang