#include "lexer/lexer.h"
#include "lexer/pipelines.h"
#include "lexer/switchers.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
namespace pangu {
namespace lexer {
pglang::PPipelineFactory create(pglang::ProductPack packer) {
    return std::make_unique<pglang::IPipelineFactory>(
        "LexerPipelineFactory",
        std::unique_ptr<pglang::ISwitcher>(new lexer::LexSwitcher()),
        lexer::LEX_PIPElINES, lexer::LEX_PIPE_ENUM, packer);
}

void analysis(const std::string &file, pglang::ProductPack packer) {
    auto factory = create(packer);
    analysis(file, std::move(factory));
}
void analysis(const std::string &file, pglang::PPipelineFactory factory) {
    std::fstream fs(file);
    if (!fs) {
        throw std::runtime_error("file " + file + " not exist");
    }
    char c;
    while (fs.get(c)) {
        factory->accept(std::unique_ptr<IData>(new lexer::DInChar(c)));
    }
    factory->accept(PData(new lexer::DInChar(0, -1)));
}

const pglang::ProductPack PACK_PRINT = [](auto factory, auto pro) {
    auto lex = ((lexer::DLex *) pro.get());
    if (lex->typeId() == lexer::ELexPipeline::Space) {
        return;
    }
};

pglang::ProductPack packNext(pglang::IPipelineFactory *factory) {
    return [ = ](auto _, auto pro) { factory->accept(std::move(pro)); };
}
} // namespace lexer
} // namespace pangu