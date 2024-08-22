#include "pgcodes/pipelines.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pipeline/assert.h"
#include "pipeline/datas.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {

void IStepPipeline::accept(IPipelineFactory *factory, PData &&data) {
    if (ignoreStepDeal(factory, data)) {
        return;
    }
    stepDeal(factory, std::move(data));
}

void PipeIf::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
    }
}

void PipeNormal::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
    }
}

void PipeBlock::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_block);
    }
}

void PipeIgnore::createProduct(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new pglang::Ignore()), DROP_PACKER);
}
void PipeIgnore::on_START(IPipelineFactory *factory, PData &&data) {
    factory->packProduct();
}

bool PipeIgnore::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}

void PipeIgnore::on_FINISH(IPipelineFactory *factory, PData &&data) {
    factory->packProduct();
}
} // namespace pgcodes
} // namespace pangu
