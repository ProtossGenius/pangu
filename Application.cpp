#include "lexer/lexer.h"
#include "pgcodes/codes.h"
#include "pipeline/declare.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <lexer/pipelines.h>
#include <ostream>

using namespace std;
using namespace pangu;
using namespace pglang;

int main(int argc, const char *argv[]) {
    if (argc == 1) {
        cout << "need file name." << endl;
        return -1;
    }
    // auto grm  = grammer::create(grammer::PACK_PRINT);
    auto code = pgcodes::create(pgcodes::PACK_PRINT);
    lexer::analysis(argv[ 1 ], lexer::packNext(code.get()));
    //  lexer ::analysis(argv[ 1 ], lexer::PACK_PRINT);
    code->status(std::cout);
}
