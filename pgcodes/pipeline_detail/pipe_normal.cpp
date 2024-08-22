#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pgcodes/pipelines.h"
#include "pipeline/assert.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {
bool PipeNormal::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    lexer::DLex *lex  = static_cast<lexer::DLex *>(data.get());
    const int    type = lex->typeId();
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return true;
    }
    // TODO: should have no code before switch.
    if (lexer::ELexPipeline::Eof == type) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return true;
    }
    if (lexer::makeSymbol("]") == *lex || lexer::makeSymbol(")") == *lex ||
        lexer::makeSymbol("}") == *lex) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return true;
    }

    if (lexer::makeSymbol("{") == *lex || lexer::makeSymbol("(") == *lex ||
        lexer::makeSymbol("[") == *lex) {
        factory->undealData(std::move(data));
        factory->choicePipeline(ECodeType::Block);
        topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
        return true;
    }
    return false;
}

ValueType getValueType(lexer::DLex *lex) {
    return lexer::isIdentifier(lex) ? ValueType::IDENTIFIER
           : lexer::isString(lex)   ? ValueType::STRING
           : lexer::isNumber(lex)   ? ValueType::NUMBER
                                    : ValueType::NOT_VALUE;
}
void PipeNormal::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    pgassert_msg(!lexer::is_keywords(lex),
                 "PipeNormal should not have keyword, keyword = " + str);
    if (lexer::makeIdentifier("return") == *lex) {
        topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
        factory->choicePipeline(ECodeType::Normal);
        factory->pushProduct(PProduct(new GCode()), pack_as_return);
    } else if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
               lexer::isString(lex)) {
        topProduct->setValue(lex->get(), getValueType(lex));
        topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
    } else if (lexer::isSymbol(lex)) {
        topProduct->setOper(lex->get());
        topProduct->setStep(int(Steps::WAIT_RIGHT));
    } else {
        factory->onFail("except identifier or symbol , but get " +
                        lex->to_string());
    }
    return;
}
void PipeNormal::on_WAIT_MID_OPER(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (!lexer::isSymbol(lex)) {
        factory->onFail("except symbol, but get : " + lex->to_string());
    }
    topProduct->setOper(lex->get());
    if (str == "++" || str == "--") {
        factory->packProduct();
        return;
    }
    topProduct->setStep(int(Steps::WAIT_RIGHT));

    return;
}
void PipeNormal::on_WAIT_RIGHT(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::is_keywords(lex)) {
        factory->undealData(std::move(data));
        factory->waitChoisePipeline();
        topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
        return;
    }
    if (lexer::isIdentifier(lex) || lexer::isNumber(lex) ||
        lexer::isString(lex)) {
        auto code = new GCode();
        code->setValue(lex->get(), getValueType(lex));
        topProduct->setRight(code);
        topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
        return;
    }
    topProduct->setStep(int(Steps::PRE_VIEW_NEXT));
    factory->undealData(std::move(data));
    factory->choicePipeline(int(ECodeType::Normal));
    factory->pushProduct(PProduct(new GCode()), pack_as_right);
    return;
}
void PipeNormal::on_PRE_VIEW_NEXT(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
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
        newTop->setStep(int(Steps::WAIT_RIGHT));
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
    code->setStep(int(Steps::WAIT_RIGHT));
    factory->choicePipeline(ECodeType::Normal);
    factory->pushProduct(PProduct(code), pack_as_right);
    return;
}
void PipeNormal::on_FINISH(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    factory->undealData(std::move(data));
    factory->packProduct();
    return;
}
} // namespace pgcodes
} // namespace pangu