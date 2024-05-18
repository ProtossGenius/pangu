#include "virtual_machine/logic_core.h"
#include "virtual_machine/machine.h"
#include <iostream>
#include <memory>
#include <vector>
int main(int argc, const char *argv[]) {
    using namespace std;
    using namespace pangu;
    auto core      = LogicCore::create();
    char code[ 5 ] = {0};
    core->run(code, 0);
    vector<std::unique_ptr<LogicCore>> list;
    list.emplace_back(std::move(core));
    for (auto &it : list) {
        it->run(code, 0);
    }

    std::unique_ptr<int>    a(new int(5));
    vector<unique_ptr<int>> hello;
    hello.emplace_back(std::move(a));
    hello[ 0 ].get();
    hello[ 0 ].get();
}
