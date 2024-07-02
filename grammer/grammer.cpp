#include "grammer/grammer.h"
#include "grammer/pipelines.h"
#include "grammer/switcher.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <iostream>
namespace pangu {
namespace grammer {
pglang::PPipelineFactory create(pglang::ProductPack packer) {
    return std::make_unique<pglang::IPipelineFactory>(
        "GrammerPipelineFactory",
        std::unique_ptr<pglang::ISwitcher>(new GrammerSwitcher()),
        grammer::GRAMMER_PIPElINES, grammer::GRAMMER_PIPE_ENUM, packer);
}

const pglang::ProductPack PACK_PRINT = [](auto factory, auto pro) {
    std::cout << pro->to_string() << std::endl;
};

pglang::ProductPack packNext(pglang::IPipelineFactory *factory) {
    return [ = ](auto _, auto pro) { factory->accept(std::move(pro)); };
}
} // namespace grammer
} // namespace pangu