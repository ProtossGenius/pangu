#include "pgcodes/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <string>
#include <utility>

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
enum class NormalStep {
    START = 0,
    WAIT_MID_OPER,
    WAIT_RIGHT,
    PRE_VIEW_NEXT,
    FINISH
};
void PipeNormal::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new GCode()));
}
grammer::ValueType getValueType(lexer::DLex *lex) {
    return lexer::isIdentifier(lex) ? grammer::ValueType::IDENTIFIER
           : lexer::isString(lex)   ? grammer::ValueType::STRING
           : lexer::isNumber(lex)   ? grammer::ValueType::NUMBER
                                    : grammer::ValueType::NOT_VALUE;
}
void PipeNormal::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    std::cout << ">>>>>>>>>>>>>" << lex->to_string() << std::endl;
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::ELexPipeline::Eof == type) {
        factory->packProduct();
        return;
    }
    switch (topProduct->getStep()) {
    case int(NormalStep::START): {
        if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
            lexer::isString(lex)) {
            topProduct->setValue(lex->get(), getValueType(lex));
            topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
        } else if (lexer::isSymbol(lex)) {
            topProduct->setOper(lex->get());
            topProduct->setStep(int(NormalStep::WAIT_RIGHT));
        } else {
            factory->onFail("except identifier or symbol , but get " +
                            lex->to_string());
        }

        return;
    }
    case int(NormalStep::WAIT_MID_OPER): {
        if (!lexer::isSymbol(lex)) {
            factory->onFail("except symbol, but get : " + lex->to_string());
        }
        topProduct->setOper(lex->get());
        topProduct->setStep(int(NormalStep::WAIT_RIGHT));

        return;
    }
    case int(NormalStep::WAIT_RIGHT): {
        if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
            lexer::isString(lex)) {
            auto code = new GCode();
            code->setValue(lex->get(), getValueType(lex));
            topProduct->setRight(code);
            topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
            return;
        }
        factory->undealData(std::move(data));
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
        factory->choicePipeline(int(ECodeType::Normal));
        return;
    }
    case int(NormalStep::PRE_VIEW_NEXT): {
        assert(isSymbol(lex));
        if (topProduct->getValueType() != grammer::ValueType::NOT_VALUE ||
            lexer::symbol_power(topProduct->getOper()) >=
                lexer::symbol_power(lex->get())) {
            if (factory->productStackSize() > 1) {
                factory->undealData(std::move(data));
                factory->packProduct();
                return;
            }

            auto &nilTop  = factory->getTopProductPtr();
            topProduct    = static_cast<GCode *>(nilTop.release());
            GCode *newTop = new GCode();
            newTop->setLeft(topProduct);
            newTop->setOper(str);
            newTop->setStep(int(NormalStep::WAIT_RIGHT));
            nilTop.reset(newTop);
            return;
        }

        auto code = new GCode();
        code->setOper(str);
        code->setLeft(topProduct->releaseRight());
        code->setStep(int(NormalStep::WAIT_RIGHT));
        factory->pushProduct(PProduct(code), pack_as_right);
        factory->choicePipeline(ECodeType::Normal);
        return;
    }
    case int(NormalStep::FINISH): {
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    default:
        factory->onFail("unknown step code : " +
                        std::to_string(topProduct->getStep()));
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
