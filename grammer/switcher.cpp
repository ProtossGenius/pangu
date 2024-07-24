#include "grammer/switcher.h"
#include "grammer/enums.h"
#include "pipeline/assert.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    if (_factory->productStackSize() == 0) {
        return _factory->choicePipeline(EGrammer::Package);
    }

    _factory->onFail("unexcept choice, cached data = " + getCachedData());
}
} // namespace grammer
} // namespace pangu