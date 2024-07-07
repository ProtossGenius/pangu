#include "grammer/datas.h"
#include "grammer/packer.h"
#include "grammer/pipelines.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
namespace pangu {
namespace grammer {

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
enum class CodeBlockStep { START = 0, FINISH };
void PipeCodeBlock::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new GCode()), packToCodeContainer);
}

void PipeCodeBlock::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {}
}

} // namespace grammer
} // namespace pangu
