#include "pgcodes/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pipeline/assert.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
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
    factory->pushProduct(PProduct(new GCode()), pack_as_right);
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
    if (lexer::makeSymbol("]") == *lex || lexer::makeSymbol(")") == *lex ||
        lexer::makeSymbol("}") == *lex) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }

    if (lexer::makeSymbol("{") == *lex || lexer::makeSymbol("(") == *lex ||
        lexer::makeSymbol("[") == *lex) {
        factory->undealData(std::move(data));
        factory->choicePipeline(ECodeType::Block);
        topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
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
        factory->choicePipeline(int(ECodeType::Normal));
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
        return;
    }
    case int(NormalStep::PRE_VIEW_NEXT): {
        pgassert(isSymbol(lex));
        if (topProduct->isValue() ||
            lexer::symbol_power(topProduct->getOper()) >=
                lexer::symbol_power(lex->get())) {
            if (factory->productStackSize() > 1) {
                factory->undealData(std::move(data));
                factory->packProduct();
                return;
            }

            GCode *newTop = new GCode();
            newTop->setOper(str);
            newTop->setStep(int(NormalStep::WAIT_RIGHT));
            auto oldTop = factory->swapTopProduct(newTop);
            newTop->setLeft(static_cast<GCode *>(oldTop.release()));
            return;
        }

        auto code = new GCode();
        code->setOper(str);
        code->setLeft(topProduct->releaseRight());
        code->setStep(int(NormalStep::WAIT_RIGHT));
        factory->choicePipeline(ECodeType::Normal);
        factory->pushProduct(PProduct(code), pack_as_right);
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

void PipeBlock::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new GCode()), pack_as_block);
}

enum class BlockStep { START = 0, WAIT_FINISH };
void PipeBlock::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    auto make_symbol_code = [](const std::string &code) {
        auto c = new GCode();
        c->setOper(code);
        return c;
    };
    switch (topProduct->getStep()) {
    case int(BlockStep::START): {
        if (lexer::makeSymbol("{") == *lex || lexer::makeSymbol("(") == *lex ||
            lexer::makeSymbol("[") == *lex) {
            topProduct->setOper(str);
            topProduct->setStep(int(BlockStep::WAIT_FINISH));
            if (lexer::makeSymbol("{") == *lex) {
                topProduct->setLeft(make_symbol_code("}"));
            } else if (lexer::makeSymbol("(") == *lex) {
                topProduct->setLeft(make_symbol_code(")"));
            } else if (lexer::makeSymbol("[") == *lex) {
                topProduct->setLeft(make_symbol_code("]"));
            }

            factory->choicePipeline(ECodeType::Normal);
            return;
        }
        factory->onFail("PipeBlock START unexcept input " + lex->to_string());
    }
    case int(BlockStep::WAIT_FINISH): {
        if (str == topProduct->getLeft()->getOper()) {
            factory->packProduct();
            return;
        }
        pgassert(topProduct->getRight() != nullptr);
        if (topProduct->getRight()->isValue() && lexer::isSymbol(lex)) {
            auto right   = topProduct->releaseRight();
            auto newProd = new GCode();
            newProd->setLeft(right);
            newProd->setStep(int(NormalStep::WAIT_MID_OPER));
            factory->undealData(std::move(data));
            factory->choicePipeline(int(ECodeType::Normal));
            factory->pushProduct(PProduct(newProd), pack_as_right);
            return;
        }
        factory->onFail("PipeBlock WAIT_FINISH unexcept input " +
                        lex->to_string());
    }
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
