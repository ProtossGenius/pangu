#pragma once
#include <memory>
namespace pangu {
namespace grammer {
#define Grm(t)                                                                 \
    class G##t;                                                                \
    typedef std::unique_ptr<G##t> P##t;
Grm(Struct);
Grm(Package);
Grm(Function);
Grm(Type);
Grm(Code);
Grm(Variable);
} // namespace grammer
} // namespace pangu