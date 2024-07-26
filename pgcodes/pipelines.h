#pragma once

#include "pgcodes/enums.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <string>
namespace pangu {
namespace pgcodes {
using namespace pglang;
static std::map<int, std::function<PipelinePtr()>> CODES_PIPELINES;
static std::map<int, std::string>                  CODES_PIPE_ENUM;
#define CODES_CLASS(type)                                                      \
    class Pipe##type : public IPipeline {                                      \
      public:                                                                  \
        void createProduct(IPipelineFactory *_factory) override;                    \
        void accept(IPipelineFactory *factory, PData &&data) override;         \
    };                                                                         \
    static Reg __reg_pipe_##type([]() {                                        \
        CODES_PIPELINES[ ECodeType::type ] =                                   \
            SinglePipelineGetter(new PipelinePtr(new Pipe##type()));           \
        CODES_PIPE_ENUM[ ECodeType::type ] = #type;                            \
    });
CODES_CLASS(If);
CODES_CLASS(Var);
CODES_CLASS(While);
CODES_CLASS(For);
CODES_CLASS(Switch);
CODES_CLASS(Goto);
CODES_CLASS(Do);
CODES_CLASS(Normal);
CODES_CLASS(Block);
CODES_CLASS(Ignore);
} // namespace pgcodes
} // namespace pangu
