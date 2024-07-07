#pragma once
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
namespace pangu {
namespace grammer {
using pglang::IPipelineFactory;
using pglang::PProduct;
void packStructToContainer(IPipelineFactory *factory, PProduct &&pro);
void packFuncDefToContainer(IPipelineFactory *factory, PProduct &&pro);
void packFuncDefToPackage(IPipelineFactory *factory, PProduct &&pro);
void packVarToContainer(IPipelineFactory *factory, PProduct &&pro);
void packToCodeContainer(IPipelineFactory *factory, PProduct &&pro);
} // namespace grammer
} // namespace pangu
