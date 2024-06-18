#include "grammer/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <utility>

namespace pangu {
namespace grammer {
void packClassToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto top = (GStructContainer *) factory->getTopProduct();
    top->addStruct(PStruct((GStruct *) pro.release()));
}

void PipeStruct::onSwitch(IPipelineFactory *_factory) {
    if (auto top = _factory->getTopProduct()) {
        _factory->pushProduct(PProduct(new GStruct()), packClassToContainer);
    }
}
void PipeStruct::accept(IPipelineFactory *factory, PData &&data) {}
} // namespace grammer
} // namespace pangu