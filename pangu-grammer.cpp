#include "lexer/datas.h"
#include "lexer/lexer.h"
#include "pipeline/declare.h"
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

    grammer::G
    lexer::analysis(argv[ 1 ], lexer::packNext());
}
