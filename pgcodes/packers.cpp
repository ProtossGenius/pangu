#include "pgcodes/packers.h"
#include "pgcodes/datas.h"
#include "pipeline/assert.h"
namespace pangu {
namespace pgcodes {
void pack_as_right(IPipelineFactory *factory, PProduct &&data) {
    GCode *code       = static_cast<GCode *>(data.release());
    GCode *topProduct = static_cast<GCode *>(factory->getTopProduct());
    pgassert(topProduct != nullptr);
    topProduct->setRight(code);
}
void pack_as_block(IPipelineFactory *factory, PProduct &&data) {
    GCode *code       = static_cast<GCode *>(data.release());
    GCode *topProduct = static_cast<GCode *>(factory->getTopProduct());
    code->releaseLeft();
    if (topProduct->getRight() != nullptr) {
        if (!topProduct->getRight()->isValue()) {
            factory->onFail("block need topProduct's right value is null or "
                            "value, right value = :" +
                            topProduct->getRight()->getValue());
        }
        code->setLeft(topProduct->releaseRight());
    }
    topProduct->setRight(code);
}
} // namespace pgcodes
} // namespace pangu