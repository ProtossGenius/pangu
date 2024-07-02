#pragma once

#include "lexer/datas.h"
#include "pipeline/switcher.h"
#include <cassert>
#include <cstring>
#include <sstream>
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
    std::string getCached() {
        std::stringstream ss;
        for (size_t i = 0; i < _cached_datas.size(); ++i) {
            ss << get(i).get() << " ";
        }
        return ss.str();
    }
};
} // namespace grammer
} // namespace pangu