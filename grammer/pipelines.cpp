#include "grammer/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <utility>

namespace pangu {
namespace grammer {
void packClassToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto top = (GStructContainer *) factory->getTopProduct();
    top->addStruct(PStruct((GStruct *) pro.release()));
}

enum StructStep {
    WAIT_TYPE = 0,
    WAIT_NAME,
    WAIT_STRUCT,
    WAIT_BODY,
    READING_VAR,
};

void PipeStruct::onSwitch(IPipelineFactory *_factory) {
    if (_factory->getTopProduct()) {
        auto ptr = new GStruct();
        ptr->setStep(StructStep::WAIT_TYPE);
        _factory->pushProduct(PProduct(ptr), packClassToContainer);
    }
    _factory->onFail("no package when create Struct.");
}

#define GET_LEX(data)                                                          \
    lexer::DLex *lex  = (lexer::DLex *) data.get();                            \
    std::string  str  = lex->get();                                            \
    int          type = lex->typeId();
#define GET_TOP(factory, Type)                                                 \
    Type *topProduct = (Type) factory->getTopProduct();

void PipeStruct::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GStruct);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case StructStep::WAIT_TYPE: {
        topProduct->setStep(StructStep::WAIT_NAME);
        return;
    }
    case StructStep::WAIT_NAME: {
        topProduct->setName(str);
        topProduct->setStep(StructStep::WAIT_STRUCT);
        return;
    }
    case StructStep::WAIT_STRUCT: {
        topProduct->setStep(StructStep::WAIT_BODY);
        return;
    }
    case StructStep::WAIT_BODY: {
        if ("{" == str) {
            topProduct->setStep(StructStep::READING_VAR);
            return;
        }
    }
    }
}
} // namespace grammer
} // namespace pangu