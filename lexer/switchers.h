#pragma once
#include "datas.h"
#include "pipeline/switcher.h"
namespace pangu {
namespace lexer {
using namespace pglang;
class LexSwitcher : ISwitcher {
  private:
    IPipeline *onChoice() override;
    DInChar   &get(size_t i) { return (DInChar &) _cached_datas[ i ]; }
};
} // namespace lexer
} // namespace pangu
