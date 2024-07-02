#include "grammer/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include "pipeline/switcher.h"
#include <string>
#include <utility>

namespace pangu {
namespace grammer {
using lexer::makeIdentifier;
using lexer::makeSpace;
using lexer::makeSymbol;
void packStructToContainer(IPipelineFactory *factory, PProduct &&pro) {
    auto top = (GStructContainer *) factory->getTopProduct();
    top->addStruct(PStruct((GStruct *) pro.release()));
}
enum class StructStep {
    WAIT_TYPE = 0,
    WAIT_NAME,
    WAIT_STRUCT,
    WAIT_BODY,
    READING_VAR,
};

void PipeStruct::onSwitch(IPipelineFactory *_factory) {
    if (_factory->getTopProduct()) {
        auto ptr = new GStruct();
        ptr->setStep((int) (int) StructStep::WAIT_TYPE);
        _factory->pushProduct(PProduct(ptr), packStructToContainer);
        return;
    }
    _factory->onFail("no package when create Struct.");
}

#define GET_LEX(data)                                                          \
    lexer::DLex *lex      = (lexer::DLex *) data.get();                        \
    std::string  str      = lex->get();                                        \
    int          type     = lex->typeId();                                     \
    std::string  typeName = lexer::LEX_PIPE_ENUM[ type ];                      \
    if (lexer::ELexPipeline::Comments == type) {                               \
        return;                                                                \
    }
#define GET_TOP(factory, Type)                                                 \
    Type *topProduct = (Type *) factory->getTopProduct();

void packVarToStruct(IPipelineFactory *factory, PProduct &&pro) {
    auto ptr             = (GVariable *) pro.release();
    auto integrityResult = ptr->integrityTest();
    if (!integrityResult.empty()) {
        factory->onFail(integrityResult);
        return;
    }
    auto top = (GVarContainer *) factory->getTopProduct();
    top->addVariable(PVariable(ptr));
}
enum class VarStep {
    WAITING_NAME = 0,
    WAITING_PACKAGE,
    WAITING_POINT,
    WAITING_TYPE,
    WAITING_DETAIL,
};
void PipeStruct::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GStruct);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case (int) StructStep::WAIT_TYPE: {
        topProduct->setStep((int) StructStep::WAIT_NAME);
        return;
    }
    case (int) StructStep::WAIT_NAME: {
        topProduct->setName(str);
        topProduct->setStep((int) StructStep::WAIT_STRUCT);
        return;
    }
    case (int) StructStep::WAIT_STRUCT: {
        topProduct->setStep((int) StructStep::WAIT_BODY);
        return;
    }
    case (int) StructStep::WAIT_BODY: {
        if (makeSymbol("{") == *lex) {
            topProduct->setStep((int) (int) StructStep::READING_VAR);
            return;
        }
        factory->onFail("need '{");
    }
    case (int) StructStep::READING_VAR: {
        if (makeSymbol("}") == *lex) {
            factory->packProduct();
            return;
        }
        factory->choicePipeline(EGrammer::Variable);
        auto ptr = new GStruct();
        factory->pushProduct(PProduct(ptr), packVarToStruct);
        break;
    }
    default:
        factory->onFail("unexcept step code : " +
                        std::to_string(topProduct->getStep()));
    }
}

void PipeVariable::onSwitch(IPipelineFactory *factory) {
    if (factory->getTopProduct()) {
        return;
    }
    factory->onFail("no variable container when create Variable.");
}

void PipeVariable::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GVariable);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        if (makeSpace("\n") == *lex) {
            factory->packProduct();
        }
        return;
    }
    switch (topProduct->getStep()) {
    case (int) VarStep::WAITING_NAME: {
        if (lexer::ELexPipeline::Identifier == type) {
            topProduct->setName(str);
            topProduct->setStep((int) VarStep::WAITING_PACKAGE);
            return;
        }
        factory->onFail("except an identifier, but get " + lex->to_string());
        break;
    }
    case (int) VarStep::WAITING_PACKAGE: {
        if (makeSymbol(",") == *lex) {
            factory->packProduct();
            return;
        }
        if (lexer::ELexPipeline::Identifier == type) {
            topProduct->getType()->read(str);
            topProduct->setStep((int) VarStep::WAITING_POINT);
            return;
        }
        factory->onFail("need type.");
        break;
    }
    case (int) VarStep::WAITING_POINT: {
        if (makeSymbol(".") == *lex) {
            topProduct->setStep((int) VarStep::WAITING_TYPE);
            return;
        }
        factory->undealData(std::move(data));
        topProduct->setStep((int) VarStep::WAITING_DETAIL);
        break;
    }
    case (int) VarStep::WAITING_DETAIL: {
        if (lexer::ELexPipeline::String == type) {
            topProduct->setDetail(str);
            factory->packProduct();
            return;
        }
        if (makeSymbol(",") == *lex || makeSymbol(";") == *lex) {
            factory->packProduct();
            return;
        }
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    default:
        factory->onFail("unexcept step code : " +
                        std::to_string(topProduct->getStep()));
    }
}

void packImport(IPipelineFactory *factory, PProduct &&pro) {
    auto top = (GPackage *) factory->getTopProduct();
    top->addImport(PImport((GImport *) pro.release()));
}

void PipeImport::onSwitch(IPipelineFactory *factory) {
    if (factory->getTopProduct()) {
        auto ptr = new GImport();
        factory->pushProduct(PProduct(ptr), packStructToContainer);
        return;
    }
    factory->onFail("no package when create Struct.");
}
enum class ImportStep {
    START = 0,
    READ_PATH,
    WAIT_ALAIS,
    READ_ALAIS,
    WAIT_FINISH
};
std::string getAlias(const std::string &package) {
    auto lastFlash = package.find_last_of("/");
    if (lastFlash == std::string::npos) {
        return package;
    }
    return package.substr(lastFlash + 1);
}
void PipeImport::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GImport);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    std::cout << "PipeImport << " << data->to_string() << std::endl;
    switch (topProduct->getStep()) {
    case (int) ImportStep::START: {

        std::cout << lex->to_string() << std::endl;
        topProduct->setStep((int) ImportStep::READ_PATH);
        return;
    }
    case (int) ImportStep::READ_PATH: {
        if (lexer::ELexPipeline::String != type) {
            factory->onFail("except package path, but get " + lex->to_string());
        }
        if (str.empty()) {
            factory->onFail("package path should not be empty");
        }
        if (str[ str.size() - 1 ] == '/') {
            factory->onFail("package path shouldn't end with '/'");
        }
        topProduct->setPackage(str);
        topProduct->setStep(int(ImportStep::WAIT_ALAIS));
        return;
    }
    case (int) ImportStep::WAIT_ALAIS: {
        if (makeIdentifier("as") == *lex) {
            topProduct->setStep((int) ImportStep::READ_ALAIS);
            return;
        }
        if (makeSymbol(";") == *lex) {
            topProduct->setAlias(getAlias(topProduct->getPackage()));
            factory->packProduct();
            return;
        }

        factory->onFail("unexcept input: get " + lex->to_string());
    }
    case (int) ImportStep::READ_ALAIS: {
        if (lexer::ELexPipeline::Identifier != type) {
            factory->onFail("except a identifier but get" + lex->to_string());
        }
        topProduct->setAlias(str);
        topProduct->setStep((int) ImportStep::WAIT_FINISH);
        return;
    }
    case (int) ImportStep::WAIT_FINISH: {
        if (makeSymbol(";") != *lex) {
            factory->onFail("except ';', but get " + lex->to_string());
        }
        factory->packProduct();
        return;
    default:
        factory->onFail("unexcept step code : " +
                        std::to_string(topProduct->getStep()));
    }
    }
}

void PipePackage::onSwitch(IPipelineFactory *factory) {
    if (factory->getTopProduct()) {
        factory->onFail("package can't in another container:" +
                        factory->getTopProduct()->to_string());
    }
    auto ptr = new GPackage();
    factory->pushProduct(PProduct(ptr));
}
enum class PkgStep { START = 0, READ_PACKAGE, FINISH };
void PipePackage::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GPackage);
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case int(PkgStep::START): {
        topProduct->setStep((int) PkgStep::READ_PACKAGE);
        return;
    }
    case int(PkgStep::READ_PACKAGE): {
        if (lexer::ELexPipeline::Identifier != type) {
            factory->onFail("except package name, but get: " +
                            lex->to_string());
        }
        topProduct->setName(str);
        topProduct->setStep(int(PkgStep::FINISH));
        return;
    case int(PkgStep::FINISH): {
        if (makeSymbol(";") != *lex) {
            factory->onFail("except ';', but get :" + lex->to_string());
        }
        factory->getSwitcher()->unchoicePipeline();
        return;
    }
    }

    default:
        factory->onFail("unexcept step code: " +
                        std::to_string(topProduct->getStep()));
    }
}

void PipeIgnore::onSwitch(IPipelineFactory *_) {}
void PipeIgnore::accept(IPipelineFactory *factory, PData &&data) {
    factory->getSwitcher()->unchoicePipeline();
}

} // namespace grammer
} // namespace pangu