#pragma once

#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace pangu {
namespace grammer {
using namespace pglang;
static std::vector<PPipeline>     LEX_PIPElINES;
static std::map<int, std::string> LEX_PIPE_ENUM;
#define PIPE_CLASS(type)                                                       \
    class Pipe##name : public IPipeline {                                      \
      public:                                                                  \
        void onSwitch(IPipelineFactory *_factory) override;                    \
        void accept(IPipelineFactory *factory, PData &&data) override;         \
    };                                                                         \
    static Reg __reg_pipe_##name([]() {                                        \
        LEX_PIPElINES.emplace_back(PPipeline((IPipeline *) new Pipe##name())); \
        LEX_PIPE_ENUM[ EGrammer::type ] = #type;                               \
    });

// 注意，这里面定义的顺序和下面Pipeline定义的顺序应该相同
enum EGrammer {
    Struct = 0,
};

PIPE_CLASS(Struct);
} // namespace grammer
} // namespace pangu
