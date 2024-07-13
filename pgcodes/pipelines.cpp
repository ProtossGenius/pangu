#include "pgcodes/pipelines.h"
#include "grammer/datas.h"

namespace pangu {
namespace pgcodes {
enum class StepEnum {
    START = 0,
};
void PipeIf::onSwitch(IPipelineFactory *factory) {}

void PipeIf::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, grammer::GCode);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {}
}

} // namespace pgcodes
} // namespace pangu
