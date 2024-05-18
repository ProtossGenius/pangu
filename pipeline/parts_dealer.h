#pragma once

#include "pipeline/declare.h"
namespace pglang {

class IPartsDealer {
  public:
    virtual void accept(PParts &&parts) = 0;

  public:
    virtual ~IPartsDealer() {}

  protected:
  protected:
    IPipelineFactory *_factory;
};
} // namespace pglang
