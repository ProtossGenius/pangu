#pragma once

#include <cstdint>
namespace pangu {
class VirtualMachine {
  public:
    void run(char *code, int len);
    void pause();
    void goon();

  private:
    volatile uint64_t _ptr;
};
} // namespace pangu
