#pragma once

#include "pipeline/declare.h"
namespace pglang {

class IProductPack {
  public:
    virtual void accept(PProduct &&pro) = 0;
    virtual ~IProductPack() {}

  protected:
    PProduct &getTopProduct();

  protected:
    IPipelineFactory *_factory;
};
} // namespace pglang
