#include "lexer/lexer.h"
#include <iostream>
int main(int argc, const char *argv[]) {
    using namespace std;
    using namespace pangu;
    if (argc == 1) {
        cout << "need file name." << endl;
        return -1;
    }
    lexer::analysis(argv[ 1 ], lexer::PACK_PRINT);
}
