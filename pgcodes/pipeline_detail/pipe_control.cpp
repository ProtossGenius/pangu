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
    topProduct->setLocation(lex->location());
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
    // Don't set oper yet — WAIT_HEADER decides "for" vs "for_in"
    topProduct->setLocation(lex->location());
    topProduct->setStep(int(Steps::WAIT_HEADER));
}
void PipeFor::on_WAIT_HEADER(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    // for (init; cond; step) { body }  -- C-style
    if (lexer::makeSymbol("(") == *lex) {
        topProduct->setOper("for");
        factory->undealData(std::move(data));
        topProduct->setStep(int(Steps::WAIT_ACTION));
        factory->choicePipeline(ECodeType::Block);
        factory->pushProduct(PProduct(new GCode()), pack_as_left);
        return;
    }
    // for x in expr { body }  -- range-based
    if (type == lexer::ELexPipeline::Identifier) {
        topProduct->setOper("for_in");
        auto *varNode = new GCode();
        varNode->setValue(lex->get(), pgcodes::ValueType::IDENTIFIER);
        varNode->setLocation(lex->location());
        topProduct->setLeft(varNode);
        topProduct->setStep(int(Steps::WAIT_IN_KEYWORD));
        return;
    }
    factory->onFail("'for' expects '(' or identifier");
}
void PipeFor::on_WAIT_IN_KEYWORD(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("in") != *lex) {
        factory->onFail("'for <var>' expects 'in'");
    }
    topProduct->setStep(int(Steps::WAIT_ITER_EXPR));
}
void PipeFor::on_WAIT_ITER_EXPR(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }

    // Accept identifier or number literal as iterable
    if (type == lexer::ELexPipeline::Identifier ||
        type == lexer::ELexPipeline::Number) {
        auto *iterNode = new GCode();
        iterNode->setValue(lex->get(),
                           type == lexer::ELexPipeline::Identifier
                               ? pgcodes::ValueType::IDENTIFIER
                               : pgcodes::ValueType::NUMBER);
        iterNode->setLocation(lex->location());
        // Build: left = "in"(varNode, iterNode)
        auto *inNode = new GCode();
        inNode->setOper("in");
        inNode->setLeft(topProduct->releaseLeft());
        inNode->setRight(iterNode);
        topProduct->setLeft(inNode);
        topProduct->setStep(int(Steps::WAIT_ACTION));
        return;
    }
    factory->onFail("'for <var> in' expects identifier or number");
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
    topProduct->setLocation(lex->location());
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

// --- Case pipeline ---
// Handles `case VALUE: { ... }` and `default: { ... }`
// AST: oper="case", left=VALUE (or null for default), right=body

bool PipeCase::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeCase::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("default") == *lex) {
        topProduct->setOper("case");
        topProduct->setLocation(lex->location());
        // default has no value, skip to WAIT_COLON
        topProduct->setStep(int(Steps::WAIT_COLON));
        return;
    }
    if (lexer::makeIdentifier("case") != *lex) {
        factory->onFail("in step START, should get 'case' or 'default'");
    }
    topProduct->setOper("case");
    topProduct->setLocation(lex->location());
    topProduct->setStep(int(Steps::WAIT_VALUE));
}
void PipeCase::on_WAIT_VALUE(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    // Parse the case value expression using Normal pipeline
    factory->undealData(std::move(data));
    topProduct->setStep(int(Steps::WAIT_COLON));
    factory->choicePipeline(ECodeType::Normal);
    factory->pushProduct(PProduct(new GCode()), pack_as_left);
}
void PipeCase::on_WAIT_COLON(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (str != ":") {
        factory->onFail("expected ':' after case value, got '" + str + "'");
    }
    topProduct->setStep(int(Steps::WAIT_ACTION));
}
void PipeCase::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordAction(factory, topProduct, std::move(data), int(Steps::FINISH));
}
void PipeCase::on_FINISH(IPipelineFactory *factory, PData &&data) {
    finishKeywordStatement(factory, std::move(data));
}

// --- Match expression pipeline ---
// Handles `match(expr) { val => body; val => body; _ => body; }`
// AST: oper="match", left=condition, right=body (containing => arms)

bool PipeMatch::ignoreStepDeal(IPipelineFactory *factory, PData &data) {
    return false;
}
void PipeMatch::on_START(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GCode);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    if (lexer::makeIdentifier("match") != *lex) {
        factory->onFail("in step START, should get identifier 'match'");
    }
    topProduct->setOper("match");
    topProduct->setLocation(lex->location());
    topProduct->setStep(int(Steps::WAIT_CONDITION));
}
void PipeMatch::on_WAIT_CONDITION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordCondition(factory, topProduct, std::move(data), "match",
                          int(Steps::WAIT_ACTION));
}
void PipeMatch::on_WAIT_ACTION(IPipelineFactory *factory, PData &&data) {
    GET_TOP(factory, GCode);
    parseKeywordAction(factory, topProduct, std::move(data), int(Steps::FINISH));
}
void PipeMatch::on_FINISH(IPipelineFactory *factory, PData &&data) {
    finishKeywordStatement(factory, std::move(data));
}

} // namespace pgcodes
} // namespace pangu
