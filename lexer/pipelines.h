#pragma once

#include "lexer/datas.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
namespace pangu {
namespace lexer {
using namespace pglang;
static std::map<int, std::function<PipelinePtr()>> LEX_PIPElINES;
static std::map<int, std::string>                  LEX_PIPE_ENUM;
#define LEXER_CLASS(name, type)                                                \
    class name : public IPipeline {                                            \
      public:                                                                  \
        void onSwitch(IPipelineFactory *_factory) override {                   \
            _factory->pushProduct(std::move(std::unique_ptr<IProduct>(         \
                (IProduct *) new DLex(ELexPipeline::type))));                  \
        }                                                                      \
        void accept(IPipelineFactory *factory, PData &&data) override;         \
    };                                                                         \
    static Reg  __reg_pipe_##name([]() {                                       \
        LEX_PIPElINES[ ELexPipeline::type ] =                                 \
            SinglePipelineGetter(new PipelinePtr(new name()));                \
        LEX_PIPE_ENUM[ ELexPipeline::type ] = #type;                          \
    });                                                                       \
    inline bool is##type(DLex *lex) {                                          \
        return lex->typeId() == ELexPipeline::type;                            \
    }                                                                          \
    inline DLex make##type(const std::string &val) {                           \
        return DLex(ELexPipeline::type, val);                                  \
    }

enum ELexPipeline {
    Number = 0,
    Identifier,
    Space,
    Symbol,
    Comments,
    String,
    Macro,
    Eof
};

LEXER_CLASS(PipeNumber, Number);
LEXER_CLASS(PipeIdentifier, Identifier)
LEXER_CLASS(PipeSpace, Space)
LEXER_CLASS(PipeSymbol, Symbol)
LEXER_CLASS(PipeComments, Comments)
LEXER_CLASS(PipeString, String)
LEXER_CLASS(PipeMacro, Macro)
LEXER_CLASS(PipeEof, Eof)

} // namespace lexer
} // namespace pangu