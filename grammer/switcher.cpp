#include "grammer/switcher.h"
#include "grammer/enums.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
namespace pangu {
namespace grammer {
void GrammerSwitcher::onChoice() {
    if (_factory->getTopProduct() == nullptr) {
        return _factory->choicePipeline(EGrammer::Package);
    }

    _factory->onFail("unexcept choice, cached data = " + getCachedData());
}
} // namespace grammer
} // namespace pangu