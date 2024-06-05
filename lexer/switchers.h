#pragma once
#include "datas.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
namespace pangu {
namespace lexer {
using namespace pglang;
class LexSwitcher : public ISwitcher {
  private:
    void onChoice() override;
    void readForAnalysis(const PData &data) override {
        ((DInChar &) data).get() == '\n' ? linePos = 0, ++lineNo : ++linePos;
    }
    DInChar &get(size_t i) { return *(DInChar *) _cached_datas[ i ].get(); }

  protected:
    void onFail(const std::string &errMsg) override;

  private:
    // char's position in line.
    int linePos;
    // line number in file.
    int lineNo;
};
} // namespace lexer
} // namespace pangu
