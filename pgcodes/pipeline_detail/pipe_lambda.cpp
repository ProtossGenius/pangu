#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pgcodes/datas.h"
#include "pgcodes/enums.h"
#include "pgcodes/packers.h"
#include "pgcodes/pipelines.h"
#include "pipeline/assert.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <string>
#include <utility>

namespace pangu {
namespace pgcodes {

bool PipeLambda::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    if (lexer::ELexPipeline::Space == data->typeId()) {
        return true;
    }
    return false;
}

void PipeLambda::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::makeIdentifier("func") == *lex) {
        topProduct->setOper("func_expr");
        topProduct->setLocation(lex->location());
        topProduct->setStep(int(Steps::EXPECT_PARAMS));
        return;
    }
    factory->onFail("expected 'func' keyword for lambda, got " + str);
}

void PipeLambda::on_EXPECT_PARAMS(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::makeSymbol("(") == *lex) {
        factory->undealData(std::move(data));
        factory->choicePipeline(ECodeType::Block);
        factory->pushProduct(PProduct(new GCode()), pack_lambda_params);
        topProduct->setStep(int(Steps::EXPECT_BODY_OR_RETURN));
        return;
    }
    factory->onFail("expected '(' after func, got " + str);
}

void PipeLambda::on_EXPECT_BODY_OR_RETURN(IPipelineFactory *factory,
                                          PData          &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::makeSymbol("{") == *lex) {
        factory->undealData(std::move(data));
        factory->choicePipeline(ECodeType::Block);
        factory->pushProduct(PProduct(new GCode()), pack_lambda_body);
        topProduct->setStep(int(Steps::FINISH));
        return;
    }
    if (lexer::isIdentifier(lex)) {
        topProduct->setName(lex->get());
        topProduct->setStep(int(Steps::EXPECT_BODY));
        return;
    }
    factory->onFail("expected return type or '{' for lambda body, got " + str);
}

void PipeLambda::on_EXPECT_BODY(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::makeSymbol("{") == *lex) {
        factory->undealData(std::move(data));
        factory->choicePipeline(ECodeType::Block);
        factory->pushProduct(PProduct(new GCode()), pack_lambda_body);
        topProduct->setStep(int(Steps::FINISH));
        return;
    }
    factory->onFail("expected '{' for lambda body, got " + str);
}

void PipeLambda::on_FINISH(IPipelineFactory *factory, PData &&data) {
    factory->undealData(std::move(data));
    factory->packProduct();
}

} // namespace pgcodes
} // namespace pangu
