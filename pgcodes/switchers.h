#pragma once
#include "lexer/datas.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
namespace pangu {
namespace pgcodes {
using namespace pglang;
class CodesSwitcher : public ISwitcher {
  private:
    void onChoice() override;

  protected:
    void onFail(const std::string &errMsg) override;

  private:
    lexer::DLex *get(size_t i) {
        return (lexer::DLex *) this->_cached_datas[ i ].get();
    }
};
} // namespace pgcodes
} // namespace pangu
