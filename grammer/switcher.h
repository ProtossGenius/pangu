#pragma once

#include "lexer/datas.h"
#include "pipeline/switcher.h"
#include <cassert>
#include <cstring>
#include <stdexcept>
namespace pangu {
namespace grammer {
using namespace pglang;
class GrammerSwitcher : public ISwitcher {
  private:
    void         onChoice() override;
    void         readForAnalysis(const PData &data) override {}
    lexer::DLex &get(size_t i) {
        assert(i < _cached_datas.size());
        return *(static_cast<lexer::DLex *>(_cached_datas[ i ].get()));
    }
    void onFail(const std::string &str) override {
        throw std::runtime_error(str);
    }
};
} // namespace grammer
} // namespace pangu