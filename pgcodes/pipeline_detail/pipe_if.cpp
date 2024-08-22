
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pgcodes/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <cassert>
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {

bool PipeIf::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeIf::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("if") != *lex) {
        factory->onFail("in step START, should get identifier 'if'");
    }
    topProduct->setOper("if");
    topProduct->setStep(int(Steps::WAIT_CONDITION));
    return;
}
void PipeIf::on_WAIT_CONDITION(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeSymbol("(") != *lex) {
        factory->onFail("in step WAIT_CONDITION, should get symbol '('");
    }
    factory->undealData(std::move(data));
    topProduct->setStep(int(Steps::WAIT_ACTION));
    factory->choicePipeline(ECodeType::Block);
    factory->pushProduct(PProduct(new GCode()), pack_as_left);
    return;
}
void PipeIf::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    factory->undealData(std::move(data));
    if (lexer::makeSymbol("{") == *lex) {
        factory->choicePipeline(ECodeType::Block);
    } else {
        factory->choicePipeline(ECodeType::Normal);
    }
    topProduct->setStep(int(Steps::WAIT_ELSE));
    factory->pushProduct(PProduct(new GCode()), pack_as_if_action);
    return;
}
void PipeIf::on_WAIT_ELSE(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer ::makeSymbol(";") == *lex) {
        return;
    }
    if (lexer::makeIdentifier("else") == *lex) {
        topProduct->setStep(int(Steps::WAIT_ELSE_CONDITION));
        return;
    }

    factory->undealData(std::move(data));
    topProduct->setStep(int(Steps::FINISH));
    return;
}

void PipeIf::on_WAIT_ELSE_CONDITION(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("if") == *lex) {
        factory->choicePipeline(ECodeType::If);
    } else {
        factory->waitChoisePipeline();
    }
    topProduct->setStep(int(Steps::FINISH));
    factory->setNextPacker(pack_as_if_else);
    factory->undealData(std::move(data));
    return;
}
void PipeIf::on_FINISH(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    factory->undealData(PData(lexer::makeSymbolPtr(";")));
    factory->undealData(std::move(data));
    factory->packProduct();
    return;
}

} // namespace pgcodes
} // namespace pangu