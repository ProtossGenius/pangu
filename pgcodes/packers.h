#pragma once
#include "pipeline/declare.h"
namespace pangu {
namespace pgcodes {
using namespace pglang;
void pack_as_right(IPipelineFactory *factory, PProduct &&data);
void pack_as_block(IPipelineFactory *factory, PProduct &&data);
} // namespace pgcodes
} // namespace pangu