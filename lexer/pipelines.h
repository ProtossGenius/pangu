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
        void createProduct(IPipelineFactory *_factory) override {              \
            _factory->pushProduct(std::unique_ptr<IProduct>(                   \
                (IProduct *) new DLex(ELexPipeline::type)));                   \
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
    }                                                                          \
    inline DLex *make##type##Ptr(const std::string &val) {                     \
        return new DLex(ELexPipeline::type, val);                              \
    }

enum ELexPipeline {
    Number = 0,
    Identifier,
    Space,
    Symbol,
    Comments,
    String,
    Macro,
    Eof,
    Char
};

LEXER_CLASS(PipeNumber, Number);
LEXER_CLASS(PipeIdentifier, Identifier)
LEXER_CLASS(PipeSpace, Space)
LEXER_CLASS(PipeSymbol, Symbol)
LEXER_CLASS(PipeComments, Comments)
// PipeString: manual class (not LEXER_CLASS) to support escape state tracking.
// The flag-based approach prevents processed escape results (e.g. \\ → \) from
// being re-interpreted as escape prefixes.
class PipeString : public IPipeline {
    bool _escape_pending  = false; // saw unprocessed backslash
    bool _in_multi_escape = false; // in octal/hex multi-char escape
  public:
    void createProduct(IPipelineFactory *_factory) override {
        _escape_pending  = false;
        _in_multi_escape = false;
        _factory->pushProduct(
            std::unique_ptr<IProduct>((IProduct *) new DLex(ELexPipeline::String)));
    }
    void accept(IPipelineFactory *factory, PData &&data) override;
};
static Reg __reg_pipe_PipeString([]() {
    LEX_PIPElINES[ELexPipeline::String] =
        SinglePipelineGetter(new PipelinePtr(new PipeString()));
    LEX_PIPE_ENUM[ELexPipeline::String] = "String";
});
inline bool   isString(DLex *lex) { return lex->typeId() == ELexPipeline::String; }
inline DLex   makeString(const std::string &val) { return DLex(ELexPipeline::String, val); }
inline DLex  *makeStringPtr(const std::string &val) {
    return new DLex(ELexPipeline::String, val);
}
LEXER_CLASS(PipeMacro, Macro)
LEXER_CLASS(PipeEof, Eof)

// Char literal pipeline: 'x' → produces NUMBER token with ASCII value
class PipeChar : public IPipeline {
  public:
    void createProduct(IPipelineFactory *_factory) override {
        _factory->pushProduct(std::unique_ptr<IProduct>(
            (IProduct *) new DLex(ELexPipeline::Number)));
    }
    void accept(IPipelineFactory *factory, PData &&data) override;
};
static Reg __reg_pipe_PipeChar([]() {
    LEX_PIPElINES[ELexPipeline::Char] =
        SinglePipelineGetter(new PipelinePtr(new PipeChar()));
    LEX_PIPE_ENUM[ELexPipeline::Char] = "Char";
});

} // namespace lexer
} // namespace pangu