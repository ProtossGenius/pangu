#include "grammer/grammer.h"
#include "lexer/datas.h"
#include "lexer/lexer.h"
#include "pipeline/declare.h"
#include <cassert>
#include <iostream>
#include <lexer/pipelines.h>
#include <memory>
#include <ostream>
#include <stdexcept>

using namespace std;
using namespace pangu;
using namespace pglang;

int main(int argc, const char *argv[]) {

    if (argc == 1) {
        cout << "need file name." << endl;
        return -1;
    }

    try {
        auto grm = grammer::create(grammer::PACK_PRINT);
        lexer::analysis(argv[ 1 ], lexer::packNext(grm.get()));
    } catch (std::runtime_error e) {
        cout << e.what() << endl;
    }
}
