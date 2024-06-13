#include "grammer/pipelines.h"

namespace pangu {
namespace grammer {
    void PipeStruct::onSwitch(IPipelineFactory *_factory) {
        if (auto top = _factory->getTopProduct()) {

        }
    }
    void PipeStruct::accept(IPipelineFactory *factory, PData &&data) {}
} // namespace grammer
} // namespace pangu