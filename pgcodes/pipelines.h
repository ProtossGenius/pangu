#pragma once

#include "pgcodes/enums.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <string>
namespace pangu {
namespace pgcodes {
using namespace pglang;
static std::map<int, std::function<PipelinePtr()>> CODES_PIPELINES;
static std::map<int, std::string>                  CODES_PIPE_ENUM;
#define CODES_CLASS(type, ...)                                                 \
    class Pipe##type : public IPipeline {                                      \
      public:                                                                  \
        void createProduct(IPipelineFactory *_factory) override;               \
        void accept(IPipelineFactory *factory, PData &&data) override;         \
                                                                               \
      public:                                                                  \
        enum Steps { START = 0, __VA_ARGS__, FINISH };                         \
                                                                               \
      private:                                                                 \
        void before(IPipelineFactory *factory, PData &&data) {}                \
        void on##__VA__ARGS__(IPipelineFactory *factory, PData &&data) {}      \
    };                                                                         \
    static Reg __reg_pipe_##type([]() {                                        \
        CODES_PIPELINES[ ECodeType::type ] =                                   \
            SinglePipelineGetter(new PipelinePtr(new Pipe##type()));           \
        CODES_PIPE_ENUM[ ECodeType::type ] = "Pipe" #type;                     \
    });
CODES_CLASS(If, M1, M2);
CODES_CLASS(Var, A);
CODES_CLASS(While, A);
CODES_CLASS(For, A);
CODES_CLASS(Switch, A);
CODES_CLASS(Goto, A);
CODES_CLASS(Do, A);
CODES_CLASS(Normal, A);
CODES_CLASS(Block, A);
CODES_CLASS(Ignore, A);
} // namespace pgcodes
} // namespace pangu
