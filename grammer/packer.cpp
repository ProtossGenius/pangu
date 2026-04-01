#include "grammer/packer.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
namespace pangu {
namespace grammer {

void packStructToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto top = static_cast<GPackage *>(factory->getTopProduct());
    top->structs.addStruct(PStruct(static_cast<GStruct *>(pro.release())));
}

void packFuncDefToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr             = static_cast<GFuncDef *>(pro.release());
    auto integrityResult = ptr->integrityTest();
    if (!integrityResult.empty()) {
        factory->onFail(integrityResult);
        return;
    }
    auto top = dynamic_cast<GTypeFunctContainer *>(factory->getTopProduct());
    top->addFunction(PFuncDef(ptr));
}

void packFuncDefToPackage(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr             = static_cast<GFuncDef *>(pro.release());
    auto integrityResult = ptr->integrityTest();
    if (!integrityResult.empty()) {
        factory->onFail(integrityResult);
        return;
    }
    auto top = static_cast<GPackage *>(factory->getTopProduct());
    top->function_defs.addFunction(PFuncDef(ptr));
}

void packFuncToPackage(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr             = static_cast<GFunction *>(pro.release());
    auto integrityResult = ptr->integrityTest();
    if (!integrityResult.empty()) {
        factory->onFail(integrityResult);
        return;
    }
    auto top = static_cast<GPackage *>(factory->getTopProduct());
    top->functions.addFunction(PFunction(ptr));
}

void packTypeDefToPackage(IPipelineFactory *factory, PProduct &&pro) {
    auto top = static_cast<GPackage *>(factory->getTopProduct());
    top->type_defs.addTypeDef(PTypeDef(static_cast<GTypeDef *>(pro.release())));
}

void packImplToPackage(IPipelineFactory *factory, PProduct &&pro) {
    auto *impl = static_cast<GImpl *>(pro.get());
    auto  top  = static_cast<GPackage *>(factory->getTopProduct());
    // Transfer methods from impl to package's function container
    for (auto &method : impl->methods()) {
        top->functions.addFunction(std::move(method));
    }
    top->impls.addImpl(PImpl(static_cast<GImpl *>(pro.release())));
}

void packVarToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr = static_cast<GVarDef *>(pro.release());
    auto top = static_cast<GVarDefContainer *>(factory->getTopProduct());
    top->addVariable(PVarDef(ptr));
}

} // namespace grammer
} // namespace pangu
