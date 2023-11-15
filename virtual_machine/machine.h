#pragma once
#include <cstdint>
#include <memory>
namespace pangu {
class LogicCore;
class HeapMemory;

class VirtualMachine {
  public:
    void run(char *code, int len);

  public:
    std::unique_ptr<HeapMemory>       &getHeap() { return _memory; }
    const std::unique_ptr<HeapMemory> &getHeap() const { return _memory; }

  private:
    std::unique_ptr<HeapMemory> _memory;
};
} // namespace pangu
