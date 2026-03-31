#include "lexer/lexer.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "lexer/switchers.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <vector>
namespace pangu {
namespace lexer {
pglang::PPipelineFactory create(pglang::ProductPack packer) {
    auto ptr = new IPipelineFactory(
        "LexerPipelineFactory",
        std::unique_ptr<pglang::ISwitcher>(new lexer::LexSwitcher()),
        lexer::LEX_PIPElINES, lexer::LEX_PIPE_ENUM, packer);
    addOnTerminalFuncs([ = ]() { ptr->status(std::cout); });
    return pglang::PPipelineFactory(ptr);
}

void analysis(const std::string &file, pglang::ProductPack packer) {
    auto factory = create(packer);
    analysis(file, std::move(factory));
}
void analysis(const std::string &file, pglang::PPipelineFactory factory) {
    std::fstream fs(file, std::ios::in | std::ios::binary);
    if (!fs) {
        throw std::runtime_error("file " + file + " not exist");
    }

    const std::string content((std::istreambuf_iterator<char>(fs)),
                              std::istreambuf_iterator<char>());

    std::vector<std::string> lines;
    std::string              current_line;
    for (char ch : content) {
        if (ch == '\n') {
            if (!current_line.empty() && current_line.back() == '\r') {
                current_line.pop_back();
            }
            lines.push_back(current_line);
            current_line.clear();
            continue;
        }
        current_line.push_back(ch);
    }
    if (!current_line.empty() && current_line.back() == '\r') {
        current_line.pop_back();
    }
    lines.push_back(current_line);
    if (lines.empty()) {
        lines.push_back("");
    }

    int line   = 1;
    int column = 1;
    for (char ch : content) {
        const std::string &line_text = lines[ size_t(line - 1) ];
        factory->accept(std::unique_ptr<IData>(
            new lexer::DInChar(ch, 0, SourceLocation{file, line, column, line_text})));
        if (ch == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
    }
    const std::string eof_line_text =
        lines[ size_t(std::min<int>(line, int(lines.size())) - 1) ];
    factory->accept(PData(
        new lexer::DInChar(0, -1, SourceLocation{file, line, column, eof_line_text})));
}

const pglang::ProductPack PACK_PRINT = [](auto factory, auto pro) {
    auto lex = ((lexer::DLex *) pro.get());
    if (lex->typeId() == lexer::ELexPipeline::Space) {
        return;
    }
    std::cout << "type = " << lex->to_string() << std::endl;
};

pglang::ProductPack packNext(pglang::IPipelineFactory *factory) {
    return [ = ](auto _, auto pro) { factory->accept(std::move(pro)); };
}
} // namespace lexer
} // namespace pangu
