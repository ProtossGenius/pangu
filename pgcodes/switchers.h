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
    void readForAnalysis(const PData &data) override {
        auto *lex = static_cast<lexer::DLex *>(data.get());
        _last_location = lex->location();
        _last_width    = lex->highlightWidth();
    }

  protected:
    void onFail(const std::string &errMsg) override;

  private:
    lexer::DLex *get(size_t i) {
        return (lexer::DLex *) this->_cached_datas[ i ].get();
    }

  private:
    lexer::SourceLocation _last_location;
    size_t                _last_width = 1;
};
} // namespace pgcodes
} // namespace pangu
