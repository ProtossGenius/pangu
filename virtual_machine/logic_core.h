#pragma once
#include <memory>
namespace pangu {
enum CoreStatus {
    READY,
    RUNNING,
    PAUSE,
    END,
};

class VirtualMachine;
class LogicCore {
    friend class VirtualMachine;

  private:
    virtual void run(char *ptr, size_t start) = 0;

  public:
    virtual ~LogicCore() {}

  private:
    // std::unique_ptr<VirtualMachine> _vm;
  public:
    static std::unique_ptr<LogicCore> create();
};
} // namespace pangu