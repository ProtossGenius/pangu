#include "pgcodes/packers.h"
#include "grammer/datas.h"

namespace pangu {
namespace pgcodes {
using grammer::GCode;
void pack_as_right(IPipelineFactory *factory, PProduct &&data) {
    GCode *code       = static_cast<GCode *>(data.release());
    GCode *topProduct = static_cast<GCode *>(factory->getTopProduct());
    topProduct->setRight(code);
}
} // namespace pgcodes
} // namespace pangu