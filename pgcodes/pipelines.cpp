#include "pgcodes/pipelines.h"
#include "grammer/datas.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/pipeline.h"

namespace pangu {
namespace pgcodes {
using grammer::GCode;
#define GET_LEX(data)                                                          \
    lexer::DLex *lex      = static_cast<lexer::DLex *>(data.get());            \
    std::string  str      = lex->get();                                        \
    int          type     = lex->typeId();                                     \
    std::string  typeName = lexer::LEX_PIPE_ENUM[ type ];                      \
    if (lexer::ELexPipeline::Comments == type) {                               \
        return;                                                                \
    }
#define GET_TOP(factory, Type)                                                 \
    Type *topProduct = static_cast<Type *>(factory->getTopProduct());

void pack_as_next(IPipelineFactory *factory, PProduct &&data) {
    GCode* code = static_cast<GCode*>(data.release());
    GCode* topProduct = static_cast<GCode*>(factory->getTopProduct());
    
}
enum class StepEnum {
    START = 0,
};
void PipeIf::onSwitch(IPipelineFactory *factory) {}

void PipeIf::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {}
}

void PipeVar::onSwitch(IPipelineFactory *factory) {}
void PipeVar::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}
void PipeWhile::onSwitch(IPipelineFactory *factory) {}
void PipeWhile::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeFor::onSwitch(IPipelineFactory *factory) {}
void PipeFor::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeSwitch::onSwitch(IPipelineFactory *factory) {}
void PipeSwitch::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeGoto::onSwitch(IPipelineFactory *factory) {}
void PipeGoto::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeDo::onSwitch(IPipelineFactory *factory) {}
void PipeDo::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeNormal::onSwitch(IPipelineFactory *factory) {}
void PipeNormal::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeBlock::onSwitch(IPipelineFactory *factory) {}
void PipeBlock::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeIgnore::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new grammer::GIgnore()),
                         [](auto a, auto b) {});
}
void PipeIgnore::accept(IPipelineFactory *factory, PData &&data) {
    factory->packProduct();
}

} // namespace pgcodes
} // namespace pangu
