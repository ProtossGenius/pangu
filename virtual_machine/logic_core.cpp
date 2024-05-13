#include "logic_core.h"
#include "functional"
#include <iostream>
namespace pangu {
typedef std::function<void(char *code, size_t &ptr, LogicCore &logicCore)>
    AsmFunc;

AsmFunc *getAsmFuncs();

class LogicCoreImpl : public LogicCore {
  public:
    void run(char *code, size_t ptr) override {
        const static auto asm_table = getAsmFuncs();
        const size_t      cmdCode   = size_t(code[ ptr++ ]);
        asm_table[ cmdCode ](code, ptr, *this);
    }
    ~LogicCoreImpl() {}
};

std::unique_ptr<LogicCore> LogicCore::create() {
    return std::unique_ptr<LogicCore>(new LogicCoreImpl());
}

AsmFunc *getAsmFuncs() {
    static AsmFunc asm_table[ 256 ];
    asm_table[ 0 ] = [](auto code, size_t &ptr, auto &core) {
        std::cout << "hello world!" << std::endl;
    };
    return asm_table;
}

} // namespace pangu