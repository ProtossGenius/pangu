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
Grm(FuncDef);
Grm(Type);
Grm(TypeDef);
Grm(Code);
Grm(Variable);
Grm(Import);
Grm(VarContainer);
} // namespace grammer
} // namespace pangu