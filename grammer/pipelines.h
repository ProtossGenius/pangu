#pragma once

#include "grammer/enums.h"
#include "pgcodes/codes.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <string>
namespace pangu {
namespace grammer {
using namespace pglang;
static std::map<int, std::function<PipelinePtr()>> GRAMMER_PIPElINES;
static std::map<int, std::string>                  GRAMMER_PIPE_ENUM;
#define GRAMMER_CLASS(type)                                                    \
    class Pipe##type : public IPipeline {                                      \
      public:                                                                  \
        void createProduct(IPipelineFactory *_factory) override;                    \
        void accept(IPipelineFactory *factory, PData &&data) override;         \
    };                                                                         \
    static Reg __reg_pipe_##type([]() {                                        \
        GRAMMER_PIPElINES[ EGrammer::type ] =                                  \
            SinglePipelineGetter(new PipelinePtr(new Pipe##type()));           \
        GRAMMER_PIPE_ENUM[ EGrammer::type ] = #type;                           \
    });

// 注意，Pipeline定义的顺序和EGrammer相同
GRAMMER_CLASS(Package);
GRAMMER_CLASS(Import);
GRAMMER_CLASS(TypeDef);
GRAMMER_CLASS(Struct);
GRAMMER_CLASS(Variable);
GRAMMER_CLASS(Ignore);
GRAMMER_CLASS(VarArray);
GRAMMER_CLASS(TypeFunc);
GRAMMER_CLASS(Func);

static Reg __reg_pipe_CodeBlock([]() {
    GRAMMER_PIPElINES[ EGrammer::CodeBlock ] = []() {
        return PipelinePtr(pgcodes::create(USE_PARENT_PACKER).release());
    };
    GRAMMER_PIPE_ENUM[ EGrammer::CodeBlock ] = "CodeBlock";
});
} // namespace grammer
} // namespace pangu
