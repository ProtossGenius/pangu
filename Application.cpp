#include "lexer/datas.h"
#include "lexer/switchers.h"
#include "pipeline/declare.h"
#include <fstream>
#include <iostream>
#include <lexer/pipelines.h>
#include <memory>
#include <vector>
int main(int argc, const char *argv[]) {
    using namespace std;
    using namespace pangu;
    using namespace pglang;
    ProductPack packer = [](auto factory, auto pro) {
        auto lex = ((lexer::DLex *) pro.get());
        if (lex->typeId() == lexer::ELexPipeline::Space) {
            return;
        }
        std::cout << "type = <" << lexer::LEX_PIPE_ENUM[ lex->typeId() ]
                  << "> content = \n"
                  << lex->get() << std::endl;
    };
    PPipelineFactory factory = make_unique<IPipelineFactory>(
        std::unique_ptr<lexer::ISwitcher>(new lexer::LexSwitcher()),
        lexer::LEX_PIPElINES, packer);
    fstream fs("../Application.cpp");
    char    c;
    while (fs.get(c)) {
        factory->accept(std::unique_ptr<IData>(new lexer::DInChar(c)));
    }
}
