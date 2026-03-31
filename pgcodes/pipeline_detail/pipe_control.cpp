#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pgcodes/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {
namespace {

void finishKeywordStatement(IPipelineFactory *factory, PData &&data) {
    factory->undealData(PData(lexer::makeSymbolPtr(";")));
    factory->undealData(std::move(data));
    factory->packProduct();
}

void parseKeywordCondition(IPipelineFactory *factory, GCode *topProduct,
                           PData &&data, const std::string &keyword,
                           int next_step) {
    GET_LEX(data);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeSymbol("(") != *lex) {
        factory->onFail("keyword '" + keyword + "' needs '('");
    }
    factory->undealData(std::move(data));
    topProduct->setStep(next_step);
    factory->choicePipeline(ECodeType::Block);
    factory->pushProduct(PProduct(new GCode()), pack_as_left);
}

void parseKeywordAction(IPipelineFactory *factory, GCode *topProduct,
                        PData &&data, int finish_step) {
    GET_LEX(data);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    factory->undealData(std::move(data));
    if (lexer::makeSymbol("{") == *lex) {
        factory->choicePipeline(ECodeType::Block);
    } else {
        factory->choicePipeline(ECodeType::Normal);
    }
    topProduct->setStep(finish_step);
    factory->pushProduct(PProduct(new GCode()), pack_as_right);
}

} // namespace

bool PipeWhile::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeWhile::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("while") != *lex) {
        factory->onFail("in step START, should get identifier 'while'");
    }
    topProduct->setOper("while");
    topProduct->setStep(int(Steps::WAIT_CONDITION));
}
void PipeWhile::on_WAIT_CONDITION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordCondition(factory, topProduct, std::move(data), "while",
                          int(Steps::WAIT_ACTION));
}
void PipeWhile::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordAction(factory, topProduct, std::move(data), int(Steps::FINISH));
}
void PipeWhile::on_FINISH(IPipelineFactory *factory, PData &&data) {
    finishKeywordStatement(factory, std::move(data));
}

bool PipeFor::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeFor::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("for") != *lex) {
        factory->onFail("in step START, should get identifier 'for'");
    }
    topProduct->setOper("for");
    topProduct->setStep(int(Steps::WAIT_HEADER));
}
void PipeFor::on_WAIT_HEADER(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordCondition(factory, topProduct, std::move(data), "for",
                          int(Steps::WAIT_ACTION));
}
void PipeFor::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordAction(factory, topProduct, std::move(data), int(Steps::FINISH));
}
void PipeFor::on_FINISH(IPipelineFactory *factory, PData &&data) {
    finishKeywordStatement(factory, std::move(data));
}

bool PipeSwitch::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeSwitch::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("switch") != *lex) {
        factory->onFail("in step START, should get identifier 'switch'");
    }
    topProduct->setOper("switch");
    topProduct->setStep(int(Steps::WAIT_CONDITION));
}
void PipeSwitch::on_WAIT_CONDITION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordCondition(factory, topProduct, std::move(data), "switch",
                          int(Steps::WAIT_ACTION));
}
void PipeSwitch::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordAction(factory, topProduct, std::move(data), int(Steps::FINISH));
}
void PipeSwitch::on_FINISH(IPipelineFactory *factory, PData &&data) {
    finishKeywordStatement(factory, std::move(data));
}

} // namespace pgcodes
} // namespace pangu
