#pragma once

#include "pgcodes/enums.h"
#include "pipeline/declare.h"
#include "pipeline/macro.h"
#include "pipeline/pipeline.h"
#include <functional>
#include <map>
#include <string>
#include <utility>
namespace pangu {
namespace pgcodes {
using namespace pglang;
static std::map<int, std::function<PipelinePtr()>> CODES_PIPELINES;
static std::map<int, std::string>                  CODES_PIPE_ENUM;
class IStepPipeline : public IPipeline {
  public:
    void accept(IPipelineFactory *factory, PData &&data) override;

  private:
    virtual bool ignoreStepDeal(IPipelineFactory *factory, PData &data) {
        return false;
    }
    virtual void stepDeal(IPipelineFactory *factory, PData &&data) = 0;
};

#define CODES_CLASS(type, ...)                                                 \
    class Pipe##type : public IStepPipeline {                                  \
      public:                                                                  \
        void createProduct(IPipelineFactory *_factory) override;               \
                                                                               \
      public:                                                                  \
        enum Steps { START = 0, __VA_OPT__(__VA_ARGS__, ) FINISH };            \
                                                                               \
      private:                                                                 \
        bool ignoreStepDeal(IPipelineFactory *factory, PData &data) override;  \
        __VA_OPT__(EXPAND(STEP_FUNCS(__VA_ARGS__)))                            \
        void on_FINISH(IPipelineFactory *factory, PData &&data);               \
        void on_START(IPipelineFactory *factory, PData &&data);                \
        void stepDeal(IPipelineFactory *factory, PData &&data) override {      \
            int step = factory->getTopProduct()->getStep();                    \
            switch (step) {                                                    \
            case Steps::START:                                                 \
                return on_START(factory, std::move(data));                     \
            case Steps::FINISH:                                                \
                return on_FINISH(factory, std::move(data));                    \
                __VA_OPT__(EXPAND(STEP_SWITCHS(__VA_ARGS__)))                  \
            default:                                                           \
                factory->onFail("unknown step value = " +                      \
                                std::to_string(step));                         \
            }                                                                  \
        }                                                                      \
    };                                                                         \
    static Reg __reg_pipe_##type([]() {                                        \
        CODES_PIPELINES[ ECodeType::type ] =                                   \
            SinglePipelineGetter(new PipelinePtr(new Pipe##type()));           \
        CODES_PIPE_ENUM[ ECodeType::type ] = "Pipe" #type;                     \
    });

#define STEP_FUNCS(head, ...)                                                  \
    void on_##head(IPipelineFactory *factory, PData &&data);                   \
    __VA_OPT__(STEP_FUNCS2 PARENS(__VA_ARGS__))
#define STEP_FUNCS2(step) STEP_FUNCS

#define STEP_SWITCHS(step, ...)                                                \
    case Steps::step:                                                          \
        return on_##step(factory, std::move(data));                            \
        __VA_OPT__(STEP_SWITCHS2 PARENS(__VA_ARGS__))
#define STEP_SWITCHS2(step) STEP_SWITCHS
CODES_CLASS(If, WAIT_CONDITION, WAIT_ACTION, WAIT_ELSE, WAIT_ELSE_CONDITION);
// CODES_CLASS(Var);
// CODES_CLASS(While);
// CODES_CLASS(For);
// CODES_CLASS(Switch);
// CODES_CLASS(Goto);
// CODES_CLASS(Do);
CODES_CLASS(Normal, WAIT_MID_OPER, WAIT_RIGHT, PRE_VIEW_NEXT);
CODES_CLASS(Block, WAIT_FINISH);
CODES_CLASS(Ignore);

#define GET_LEX(data)                                                          \
    lexer::DLex *lex      = static_cast<lexer::DLex *>(data.get());            \
    std::string  str      = lex->get();                                        \
    int          type     = lex->typeId();                                     \
    std::string  typeName = lexer::LEX_PIPE_ENUM[ type ];                      \
    if (lexer::ELexPipeline::Comments == type) {                               \
        return;                                                                \
    }

} // namespace pgcodes
} // namespace pangu
