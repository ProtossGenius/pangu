#include "pipeline/pipeline.h"
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <ostream>
#include <pipeline/declare.h>
#include <utility>
#include <vector>
namespace pglang {

void USE_PARENT_PACKER(IPipelineFactory *factory, PProduct &&pro) {
    factory->packToParent(std::move(pro));
}
void DROP_PACKER(IPipelineFactory *factory, PProduct &&pro) {}
static std::vector<std::function<void()>> _terminal_functions;

void addOnTerminalFuncs(std::function<void()> funcs) {
    _terminal_functions.push_back(funcs);
}

void callAllTerminalFuncs() {
    std::cout << "######################### callAllTerminalFuncs "
                 "######################"
              << std::endl;
    for (auto it : _terminal_functions) {
        it();
    }

    auto e = std::current_exception();
    if (e) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception &ee) {
            std::cerr << "Exception: what() = " << ee.what() << std::endl;
        } catch (...) {
            std::cerr << "unknown exception caught." << std::endl;
        }
    }
    std::abort();
}

void registerTerminalFuncs() { std::set_terminate(callAllTerminalFuncs); }

} // namespace pglang