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

GCode *make_symbol_code(const std::string &code) {
    auto c = new GCode();
    c->setOper(code);
    return c;
};
bool PipeBlock::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    if (lexer::ELexPipeline::Space == data->typeId()) {
        return true;
    }
    return false;
}
void PipeBlock::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::makeSymbol("{") == *lex || lexer::makeSymbol("(") == *lex ||
        lexer::makeSymbol("[") == *lex) {
        topProduct->setOper(str);
        topProduct->setStep(int(Steps::WAIT_FINISH));
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
void PipeBlock::on_WAIT_FINISH(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (str == topProduct->getLeft()->getOper()) {
        factory->packProduct();
        return;
    }
    pgassert(topProduct->getRight() != nullptr);
    if (lexer::isSymbol(lex)) {
        auto right   = topProduct->releaseRight();
        auto newProd = new GCode();
        newProd->setLeft(right);
        newProd->setStep(int(PipeNormal::WAIT_MID_OPER));
        factory->undealData(std::move(data));
        factory->choicePipeline(int(ECodeType::Normal));
        factory->pushProduct(PProduct(newProd), pack_as_right);
        return;
    }
}

void PipeBlock::on_FINISH(IPipelineFactory *factory, PData &&data) {
    factory->undealData(std::move(data));
    factory->packProduct();
}
} // namespace pgcodes
} // namespace pangu