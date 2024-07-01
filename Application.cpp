#include "lexer/datas.h"
#include "lexer/lexer.h"
#include "pipeline/declare.h"
#include <iostream>
#include <lexer/pipelines.h>
#include <ostream>
std::string getAlias(const std::string &package) {
    auto lastFlash = package.find_last_of("/");
    if (lastFlash == std::string::npos) {
        return package;
    }
    return package.substr(lastFlash);
}
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
    lexer::analysis(argv[ 1 ], packer);

    cout << getAlias("hhhh.hhhh/hhha/aaaa") << endl << getAlias("bbbb") << endl;
}
