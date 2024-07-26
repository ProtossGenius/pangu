#include "pgcodes/codes.h"
#include "pgcodes/datas.h"
#include "pgcodes/pipelines.h"
#include "pgcodes/switchers.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <iostream>
#include <ostream>
namespace pangu {
namespace pgcodes {
pglang::PPipelineFactory create(pglang::ProductPack packer) {
    auto ptr = new pglang::IPipelineFactory(
        "CodePipelineFactory",
        std::unique_ptr<pglang::ISwitcher>(new CodesSwitcher()),
        CODES_PIPELINES, CODES_PIPE_ENUM, packer);
    pglang::addOnTerminalFuncs([ = ]() { ptr->status(std::cout); });
    return pglang::PPipelineFactory(ptr);
}

const pglang::ProductPack PACK_PRINT = [](auto factory, auto pro) {
    auto code = ((GCode *) pro.get());
    std::cout << "code = " << code->to_string() << std::endl;
};

pglang::ProductPack packNext(pglang::IPipelineFactory *factory) {
    return [ = ](auto _, auto pro) { factory->accept(std::move(pro)); };
}

} // namespace pgcodes
} // namespace pangu