#include "grammer/packer.h"
#include "grammer/datas.h"
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

void packVarToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr             = static_cast<GVariable *>(pro.release());
    auto integrityResult = ptr->integrityTest();
    if (!integrityResult.empty()) {
        factory->onFail(integrityResult);
        return;
    }
    auto top = static_cast<GVarContainer *>(factory->getTopProduct());
    top->addVariable(PVariable(ptr));
}

} // namespace grammer
} // namespace pangu