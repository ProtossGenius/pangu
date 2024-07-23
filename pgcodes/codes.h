#pragma once
#include "pipeline/declare.h"
namespace pangu {
namespace pgcodes {
pglang::PPipelineFactory create(pglang::ProductPack packer);

extern const pglang::ProductPack PACK_PRINT;
pglang::ProductPack              packNext(pglang::IPipelineFactory *factory);
} // namespace pgcodes
} // namespace pangu