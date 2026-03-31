#include "lexer/lexer.h"
#include <exception>
#include <iostream>
int main(int argc, const char *argv[]) {
    using namespace std;
    using namespace pangu;
    if (argc == 1) {
        cout << "need file name." << endl;
        return -1;
    }
    try {
        lexer::analysis(argv[ 1 ], lexer::PACK_PRINT);
    } catch (const std::exception &ex) {
        cerr << ex.what() << endl;
        return -1;
    }
    return 0;
}
