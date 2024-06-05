#pragma once

#include "pipeline/switcher.h"
namespace pangu {
namespace grammer {
using namespace pglang;
class GrammerSwitcher : public ISwitcher {

    void onChoice() override;
    void readForAnalysis(const PData &data) override {}
};
} // namespace grammer
} // namespace pangu