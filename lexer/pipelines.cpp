#include "lexer/datas.h"
#include <lexer/pipelines.h>
#include <memory>

namespace pangu {
namespace lexer {
void PipeNumber::accept(PData &&data) {
    char  c   = ((DInChar *) data.get())->get();
    DLex &lex = (DLex &) _factory->getTopProduct();
    auto &str = lex.get();
    switch (c) {
    
     case 'l': case 'L' : {
        str += c;
        _factory->packProduct();
        
     }
     
    }
    str += c;
}

} // namespace lexer
} // namespace pangu