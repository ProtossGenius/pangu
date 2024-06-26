#include "lexer/datas.h"
#include "lexer/lexer.h"
#include "pipeline/declare.h"
#include <iostream>
#include <lexer/pipelines.h>
int main(int argc, const char *argv[]) {
    using namespace std;
    using namespace pangu;
    using namespace pglang;
    if (argc == 1) {
        cout << "need file name." << endl;
        return -1;
    }
    ProductPack packer = [](auto factory, auto pro) {
        auto lex = ((lexer::DLex *) pro.get());
        if (lex->typeId() == lexer::ELexPipeline::Space) {
            return;
        }
        std::cout << "type = <" << lexer::LEX_PIPE_ENUM[ lex->typeId() ]
                  << "> content = " << lex->get() << std::endl;
    };
    lexer::analysis(argv[1], packer);
}
