#pragma once

#include "lexer/datas.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <memory>
#include <vector>
namespace pangu {
namespace lexer {
using namespace pglang;

static std::vector<PPipeline> LEX_PIPElINES;
#define PIPE_CLASS(name, type)                                                 \
    class name : IPipeline {                                                   \
      public:                                                                  \
        void onSwitch() override {                                             \
            _factory->pushProduct(std::move(std::unique_ptr<IProduct>(         \
                (IProduct *) new DLex(ELexPipeline::type))));                  \
        }                                                                      \
        void accept(PData &&data) override;                                    \
    };                                                                         \
    static Reg __reg_pipe_##name([]() {                                        \
        LEX_PIPElINES.emplace_back(PPipeline((IPipeline *) new name()));       \
    });

// 注意，这里面定义的顺序和下面Pipeline定义的顺序应该相同
enum ELexPipeline { Number = 0, Identifier, Space, Symbol, Comments, String };

PIPE_CLASS(PipeNumber, Number);
PIPE_CLASS(PipeIdentifier, Identifier)
PIPE_CLASS(PipeSpace, Space)
PIPE_CLASS(PipeSymbol, Symbol)
PIPE_CLASS(PipeComments, Comments)
PIPE_CLASS(PipeString, String)
} // namespace lexer
} // namespace pangu