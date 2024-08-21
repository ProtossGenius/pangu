#include "pgcodes/pipelines.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pipeline/assert.h"
#include "pipeline/datas.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {
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

void PipeIf::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
    }
}

enum class IfStep {
    START = 0,
    WAIT_IF,
    WAIT_CONDITION,
    WAIT_ACTION,
    WAIT_ELSE,
    WAIT_ELSE_CONDITION,
    FINISH
};
void PipeIf::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case int(IfStep::START): {
        if (lexer::makeIdentifier("if") != *lex) {
            factory->onFail("in step START, should get identifier 'if'");
        }
        topProduct->setOper("if");
        topProduct->setStep(int(IfStep::WAIT_CONDITION));
        return;
    }
    case int(IfStep::WAIT_CONDITION): {
        if (lexer::makeSymbol("(") != *lex) {
            factory->onFail("in step WAIT_CONDITION, should get symbol '('");
        }
        factory->undealData(std::move(data));
        topProduct->setStep(int(IfStep::WAIT_ACTION));
        factory->choicePipeline(ECodeType::Block);
        factory->pushProduct(PProduct(new GCode()), pack_as_left);
        return;
    }
    case int(IfStep::WAIT_ACTION): {
        factory->undealData(std::move(data));
        if (lexer::makeSymbol("{") == *lex) {
            factory->choicePipeline(ECodeType::Block);
        } else {
            factory->choicePipeline(ECodeType::Normal);
        }
        topProduct->setStep(int(IfStep::WAIT_ELSE));
        factory->pushProduct(PProduct(new GCode()), pack_as_if_action);
        return;
    }
    case int(IfStep::WAIT_ELSE): {
        if (lexer ::makeSymbol(";") == *lex) {
            return;
        }
        if (lexer::makeIdentifier("else") == *lex) {
            topProduct->setStep(int(IfStep::WAIT_ELSE_CONDITION));
            return;
        }

        factory->undealData(std::move(data));
        topProduct->setStep(int(IfStep::FINISH));
        return;
    }

    case int(IfStep::WAIT_ELSE_CONDITION): {
        if (lexer::makeIdentifier("if") == *lex) {
            factory->choicePipeline(ECodeType::If);
        } else {
            factory->waitChoisePipeline();
        }
        topProduct->setStep(int(IfStep::FINISH));
        factory->setNextPacker(pack_as_if_else);
        factory->undealData(std::move(data));
        return;
    }
    case int(IfStep::FINISH): {
        factory->undealData(PData(lexer::makeSymbolPtr(";")));
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    default:
        factory->onFail("unknow step value : " +
                        std::to_string(topProduct->getStep()));
    }
}

void PipeVar::createProduct(IPipelineFactory *factory) {}
void PipeVar::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}
void PipeWhile::createProduct(IPipelineFactory *factory) {}
void PipeWhile::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeFor::createProduct(IPipelineFactory *factory) {}
void PipeFor::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeSwitch::createProduct(IPipelineFactory *factory) {}
void PipeSwitch::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeGoto::createProduct(IPipelineFactory *factory) {}
void PipeGoto::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
}

void PipeDo::createProduct(IPipelineFactory *factory) {}
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
void PipeNormal::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
    }
}
ValueType getValueType(lexer::DLex *lex) {
    return lexer::isIdentifier(lex) ? ValueType::IDENTIFIER
           : lexer::isString(lex)   ? ValueType::STRING
           : lexer::isNumber(lex)   ? ValueType::NUMBER
                                    : ValueType::NOT_VALUE;
}
void PipeNormal::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::ELexPipeline::Eof == type) {
        factory->undealData(std::move(data));
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
        pgassert_msg(!lexer::is_keywords(lex),
                     "PipeNormal should not have keyword, keyword = " + str);
        if (lexer::makeIdentifier("return") == *lex) {
            topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
            factory->choicePipeline(ECodeType::Normal);
            factory->pushProduct(PProduct(new GCode()), pack_as_return);
        } else if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
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
        if (str == "++" || str == "--") {
            factory->packProduct();
            return;
        }
        topProduct->setStep(int(NormalStep::WAIT_RIGHT));

        return;
    }
    case int(NormalStep::WAIT_RIGHT): {
        if (lexer::is_keywords(lex)) {
            factory->undealData(std::move(data));
            factory->waitChoisePipeline();
            topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
            return;
        }
        if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
            lexer::isString(lex)) {
            auto code = new GCode();
            code->setValue(lex->get(), getValueType(lex));
            topProduct->setRight(code);
            topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
            return;
        }
        topProduct->setStep(int(NormalStep::PRE_VIEW_NEXT));
        factory->undealData(std::move(data));
        factory->choicePipeline(int(ECodeType::Normal));
        factory->pushProduct(PProduct(new GCode()), pack_as_right);
        return;
    }
    case int(NormalStep::PRE_VIEW_NEXT): {
        pgassert_msg(isSymbol(lex), lex->to_string());
        if (topProduct->isValue() ||
            lexer::symbol_power(topProduct->getOper()) >
                lexer::symbol_power(lex->get()) ||
            (lexer::symbol_power(topProduct->getOper()) ==
                 lexer::symbol_power(lex->get()) &&
             lex->get() != "=")) {
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
        if (str == "++" || str == "--") {
            topProduct->setRight(code);
            return;
        }
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

void PipeBlock::createProduct(IPipelineFactory *factory) {
    if (factory->productStackSize() == 0) {
        factory->pushProduct(PProduct(new GCode()));
    } else {
        factory->pushProduct(PProduct(new GCode()), pack_as_block);
    }
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
        if (lexer::isSymbol(lex)) {
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

void PipeIgnore::createProduct(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new pglang::Ignore()), [](auto a, auto b) {});
}
void PipeIgnore::accept(IPipelineFactory *factory, PData &&data) {
    factory->packProduct();
}

} // namespace pgcodes
} // namespace pangu
