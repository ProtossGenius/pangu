#include "logic_core.h"
#include "functional"
namespace pangu {
std::function<void(char *code, size_t ptr)> asm_table[ 255 ];
class LogicCoreImpl : public LogicCore {
  public:
    void run(char *code, size_t ptr) override {
        asm_table[ size_t(code[ ptr ]) ](code, ptr + 1);
    }
    ~LogicCoreImpl() {}
};

std::unique_ptr<LogicCore> LogicCore::create() {
    return std::unique_ptr<LogicCore>(new LogicCoreImpl());
}

} // namespace pangu