#include "pipeline/product_pack.h"
#include "pipeline/pipeline.h"
namespace pglang {
PProduct &IProductPack::getTopProduct() { return _factory->getTopProduct(); }
} // namespace pglang