#pragma once

#include "pipeline/declare.h"
namespace pglang {

class IPartsDealer {
  public:
    virtual void accept(PParts &&parts){};

  public:
    virtual ~IPartsDealer() {}

  protected:
    IPipelineFactory *_factory;
};
} // namespace pglang
