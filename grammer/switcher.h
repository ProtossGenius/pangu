#pragma once

#include "lexer/datas.h"
#include "pipeline/switcher.h"
namespace pangu {
namespace grammer {
using namespace pglang;
class GrammerSwitcher : public ISwitcher {
  private:
    void         onChoice() override;
    void         readForAnalysis(const PData &data) override {}
    lexer::DLex &get(size_t i) {
        return *(lexer::DLex *) _cached_datas[ i ].get();
    }
};
} // namespace grammer
} // namespace pangu