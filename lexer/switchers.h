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
        _last_location = static_cast<DInChar *>(data.get())->location();
    }
    DInChar &get(size_t i) {
        return *static_cast<DInChar *>(_cached_datas[ i ].get());
    }

  protected:
    void onFail(const std::string &errMsg) override;

  private:
    SourceLocation _last_location;
};
} // namespace lexer
} // namespace pangu
