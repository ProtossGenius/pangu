#include "grammer/grammer.h"
#include "grammer/datas.h"
#include "grammer/pipelines.h"
#include "grammer/switcher.h"
#include "lexer/lexer.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <iostream>
namespace pangu {
namespace grammer {
pglang::PPipelineFactory create(pglang::ProductPack packer) {
    auto ptr = new IPipelineFactory(
        "GrammerPipelineFactory",
        std::unique_ptr<pglang::ISwitcher>(new GrammerSwitcher()),
        grammer::GRAMMER_PIPElINES, grammer::GRAMMER_PIPE_ENUM, packer);
    addOnTerminalFuncs([ = ]() { ptr->status(std::cout); });
    return pglang::PPipelineFactory(ptr);
}

const pglang::ProductPack PACK_PRINT = [](auto factory, auto pro) {
    std::cout << pro->to_string() << std::endl;
};

pglang::ProductPack packNext(pglang::IPipelineFactory *factory) {
    return [ = ](auto _, auto pro) { factory->accept(std::move(pro)); };
}

std::unique_ptr<pgcodes::GCode> parseExpression(const std::string &expr_str) {
    // Wrap expression in an assignment so the parser handles full expression grammar
    std::string source = "package _interp; func _e() int { _r := " + expr_str + "; }";
    
    std::unique_ptr<GPackage> package;
    try {
        auto packer = [&](auto, auto pro) {
            package.reset(static_cast<GPackage *>(pro.release()));
        };
        auto grm = create(packer);
        auto lex_factory = lexer::create(lexer::packNext(grm.get()));
        lexer::analysisString(source, std::move(lex_factory));
    } catch (...) {
        return nullptr;
    }
    
    if (!package) return nullptr;
    auto *func = package->getFunction("_e");
    if (!func || !func->code) return nullptr;
    
    // Navigate AST: { → right(body) → ; → left(:= → right(EXPR))
    auto *code = func->code.get();
    if (code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
        code->getOper() == "{") {
        code = code->getRight();
    }
    while (code && code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
           code->getOper() == ";") {
        code = code->getLeft();
    }
    // code should be := → left: _r, right: EXPR
    if (code && code->getValueType() == pgcodes::ValueType::NOT_VALUE &&
        code->getOper() == ":=") {
        auto *expr = code->releaseRight();
        if (expr) return std::unique_ptr<pgcodes::GCode>(expr);
    }
    return nullptr;
}

} // namespace grammer
} // namespace pangu