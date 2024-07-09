#include "grammer/pipelines.h"
#include "grammer/datas.h"
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "grammer/packer.h"
#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include "pipeline/declare.h"
#include "pipeline/pipeline.h"
#include <string>
#include <utility>

namespace pangu {
namespace grammer {
#define GET_LEX(data)                                                          \
    lexer::DLex *lex      = static_cast<lexer::DLex *>(data.get());            \
    std::string  str      = lex->get();                                        \
    int          type     = lex->typeId();                                     \
    std::string  typeName = lexer::LEX_PIPE_ENUM[ type ];                      \
    if (lexer::ELexPipeline::Comments == type) {                               \
        return;                                                                \
    }
#define GET_TOP(factory, Type)                                                 \
    Type *topProduct = static_cast<Type *>(factory->getTopProduct());
using lexer::makeIdentifier;
using lexer::makeSpace;
using lexer::makeSymbol;
enum class TypeDefStep {
    WAIT_TYPE = 0,
    READ_NAME,
    READ_TYPE,
};

void dropType(IPipelineFactory *factory, PProduct &&pro) {}
void PipeTypeDef::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new GTypeDef()), dropType);
}
void PipeTypeDef::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GTypeDef);
    if (lexer::ELexPipeline::Comments == type ||
        lexer::ELexPipeline::Space == type) {
        return;
    }

    switch (topProduct->getStep()) {
    case int(TypeDefStep::WAIT_TYPE): {
        if (makeIdentifier("type") != *lex) {
            factory->onFail("except identifier 'type', but get: " +
                            lex->to_string());
        }
        topProduct->setStep(int(TypeDefStep::READ_NAME));
        return;
    }
    case int(TypeDefStep::READ_NAME): {
        if (lexer::ELexPipeline::Identifier != type) {
            factory->onFail("except type name(Identifier), but get : " +
                            lex->to_string());
        }

        topProduct->setName(str);
        topProduct->setStep(int(TypeDefStep::READ_TYPE));
        return;
    }
    case int(TypeDefStep::READ_TYPE): {
        if (lexer::ELexPipeline::Identifier != type) {
            factory->onFail("except TypeDef's type(Identifier), but get : " +
                            lex->to_string());
        }
        auto name = topProduct->name();
        factory->packProduct();
        if (str == "struct") {
            auto ptr = new GStruct();
            ptr->setName(name);
            factory->pushProduct(PProduct(ptr), packStructToContainer);
            factory->choicePipeline(EGrammer::Struct);
        } else if (str == "func") {
            factory->undealData(std::move(data));
            auto ptr = new GFuncDef();
            ptr->setName(name);
            factory->pushProduct(PProduct(ptr), packFuncDefToPackage);
            factory->choicePipeline(EGrammer::TypeFunc);
        } else {
            factory->onFail("unknow type " + str);
        }
        return;
    }

    default:
        factory->onFail("unexcept TypeDefStep, code = " +
                        std::to_string(topProduct->getStep()));
    }
}
enum class StructStep {
    WAIT_BODY = 0,
    READING_VAR,
};

void PipeStruct::onSwitch(IPipelineFactory *_factory) {
    // if (_factory->getTopProduct()) {
    //     auto ptr = new GStruct();
    //     ptr->setStep((int) (int) StructStep::WAIT_TYPE);
    //     _factory->pushProduct(PProduct(ptr), packStructToContainer);
    //     return;
    // }
}

void PipeStruct::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GStruct);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case (int) StructStep::WAIT_BODY: {
        if (makeSymbol("{") == *lex) {
            topProduct->setStep((int) StructStep::READING_VAR);
            return;
        }
        factory->onFail("need '{");
    }
    case (int) StructStep::READING_VAR: {
        if (makeSymbol(";") == *lex) {
            return;
        }
        if (makeSymbol("}") == *lex) {
            factory->packProduct();
            return;
        }
        factory->undealData(std::move(data));
        factory->choicePipeline(EGrammer::Variable);
        auto ptr = new GVarDef();
        factory->pushProduct(PProduct(ptr), packVarToContainer);
        break;
    }
    default:
        factory->onFail(
            "PipeStruct: product = " + topProduct->to_string() +
            " unexcept step code : " + std::to_string(topProduct->getStep()));
    }
}

void PipeVariable::onSwitch(IPipelineFactory *factory) {
    if (factory->getTopProduct()) {
        return;
    }
    factory->onFail("no variable container when create Variable.");
}

enum class VarStep {
    WAITING_NAME = 0,
    WAITING_PACKAGE,
    WAITING_POINT,
    WAITING_TYPE,
    WAITING_DETAIL,
    WAITING_FINISH,
};
void PipeVariable::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GVarDef);
    // ignore space.
    if (lexer::ELexPipeline::Space == type) {
        if (makeSpace("\n") == *lex) {
            factory->undealData(std::move(data));
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
        factory->onFail("PipeVariable: need type.");
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
    case (int) VarStep::WAITING_TYPE: {
        topProduct->getType()->read(str);
        topProduct->setStep(int(VarStep::WAITING_DETAIL));
        return;
    }
    case (int) VarStep::WAITING_DETAIL: {
        if (lexer::ELexPipeline::String == type) {
            topProduct->setDetail(str);
            topProduct->setStep(int(VarStep::WAITING_FINISH));
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
    case (int) VarStep::WAITING_FINISH: {
        if (makeSymbol(",") == *lex || makeSymbol(";") == *lex ||
            lexer::ELexPipeline::Comments == type) {
            factory->packProduct();
            return;
        }
        if (lexer::ELexPipeline::Space == type ||
            lexer::ELexPipeline::Symbol == type) {
            factory->undealData(std::move(data));
            factory->packProduct();
            return;
        }
        factory->onFail("wait symbol ';' or '\n' or '}', unexcept input : " +
                        lex->to_string());
    }
    default:
        factory->onFail(
            "PipeVariable: product = " + topProduct->to_string() +
            " unexcept step code : " + std::to_string(topProduct->getStep()));
    }
}

void packImport(IPipelineFactory *factory, PProduct &&pro) {
    auto top = static_cast<GPackage *>(factory->getTopProduct());
    top->addImport(PImport((GImport *) pro.release()));
}

void PipeImport::onSwitch(IPipelineFactory *factory) {
    if (factory->getTopProduct()) {
        auto ptr = new GImport();
        factory->pushProduct(PProduct(ptr), packImport);
        return;
    }
    factory->onFail("no package when create import.");
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
    switch (topProduct->getStep()) {
    case (int) ImportStep::START: {
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
        factory->onFail(
            "PipeImport: product = " + topProduct->to_string() +
            " unexcept step code : " + std::to_string(topProduct->getStep()));
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
enum class PkgStep { START = 0, READ_PACKAGE, FINISH, READ_BODY };
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
    }
    case int(PkgStep::FINISH): {
        if (makeSymbol(";") != *lex) {
            factory->onFail("except ';', but get :" + lex->to_string());
        }
        topProduct->setStep(int(PkgStep::READ_BODY));
        // factory->unchoicePipeline();
        return;
    }
    case int(PkgStep::READ_BODY): {
        if (type == lexer::ELexPipeline::Eof) {
            factory->packProduct();
            return;
        }
        break;
    }
    default:
        factory->onFail(
            "PipePackage: product = " + topProduct->to_string() +
            " unexcept step code: " + std::to_string(topProduct->getStep()));
    }
    factory->undealData(std::move(data));
    if (lexer::makeIdentifier("import") == *lex) {
        return factory->choicePipeline(EGrammer::Import);
    }

    if (lexer::makeIdentifier("type") == *lex) {
        return factory->choicePipeline(EGrammer::TypeDef);
    }
    if (lex->typeId() == lexer::ELexPipeline::Space ||
        lex->typeId() == lexer::ELexPipeline::Comments ||
        makeSymbol(";") == *lex) { // TODO: think another symbol?
        return factory->choicePipeline(EGrammer::Ignore);
    }
    factory->onFail("package can't choice pipeline: " + lex->to_string());
}

void PipeIgnore::onSwitch(IPipelineFactory *factory) {
    factory->pushProduct(PProduct(new GIgnore()), [](auto a, auto b) {});
}
void PipeIgnore::accept(IPipelineFactory *factory, PData &&data) {
    factory->packProduct();
}

void PipeVarArray::onSwitch(IPipelineFactory *factory) {}
enum class VarArrayStep { START = 0, READ_SINGLE, READ_MULTI, FINISH };
void PipeVarArray::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GVarDefContainer);

    if (int(VarArrayStep::FINISH) == topProduct->getStep()) {
        factory->undealData(std::move(data));
        factory->packProduct();
        return;
    }
    if (lexer::ELexPipeline::Space == type) {
        return;
    }
    switch (topProduct->getStep()) {
    case int(VarArrayStep::START): {
        if (makeSymbol("(") == *lex) {
            topProduct->setStep(int(VarArrayStep::READ_MULTI));
            return;
        }
        if (lexer::ELexPipeline::Identifier == type) {
            factory->undealData(std::move(data));
            factory->choicePipeline(EGrammer::Variable);
            auto var = new GVarDef();
            var->setName("return");
            var->setStep(int(VarStep::WAITING_TYPE));
            factory->pushProduct(PProduct(var), packVarToContainer);
            topProduct->setStep(int(VarArrayStep::FINISH));
            return;
        }

        factory->onFail("unexcept input: " + lex->to_string());
        break;
    }
    case int(VarArrayStep::READ_MULTI): {
        if (makeSymbol(",") == *lex) {
            return;
        }
        if (makeSymbol(")") == *lex) {
            factory->packProduct();
            return;
        }
        factory->undealData(std::move(data));
        factory->choicePipeline(EGrammer::Variable);
        auto ptr = new GVarDef();
        factory->pushProduct(PProduct(ptr), packVarToContainer);
        return;
    }
    default:
        factory->onFail(
            "PipeVarArray: product = " + topProduct->to_string() +
            " unexcept step code : " + std::to_string(topProduct->getStep()));
    }
}

void PipeTypeFunc::onSwitch(IPipelineFactory *factory) {}
enum class FuncDefStep { READ_FUNC = 0, READ_PARAM, READ_RETURN, FINISH };
void PipeTypeFunc::accept(IPipelineFactory *factory, PData &&data) {
    GET_LEX(data);
    GET_TOP(factory, GFuncDef);
    if (topProduct->getStep() == int(FuncDefStep::FINISH)) {
        factory->undealData(std::move(data));
        factory->packProduct();
    }
    if (lexer::ELexPipeline::Space == type) {
        if (topProduct->getStep() == int(FuncDefStep::READ_RETURN) &&
            (makeSpace("\n") == *lex)) {
            factory->packProduct();
        }
        return;
    }
    switch (topProduct->getStep()) {
    case int(FuncDefStep::READ_FUNC): {
        topProduct->setStep(int(FuncDefStep::READ_PARAM));
        return;
    }
    case int(FuncDefStep::READ_PARAM): {
        factory->undealData(std::move(data));
        factory->choicePipeline(EGrammer::VarArray);
        auto ptr = new GVarDefContainer();
        factory->pushProduct(PProduct(ptr), [ = ](auto f, auto pro) {
            topProduct->params.swap(*ptr);
        });
        topProduct->setStep(int(FuncDefStep::READ_RETURN));
        return;
    }
    case int(FuncDefStep::READ_RETURN): {
        if (makeSymbol(";") == *lex) {
            factory->packProduct();
            return;
        }
        factory->undealData(std::move(data));
        factory->choicePipeline(EGrammer::VarArray);
        auto ptr = new GVarDefContainer();
        factory->pushProduct(PProduct(ptr), [ = ](auto f, auto pro) {
            topProduct->result.swap(*ptr);
        });
        topProduct->setStep(int(FuncDefStep::FINISH));
        return;
    }
    default:
        factory->onFail(
            "PipeTypeFunc: product = " + topProduct->to_string() +
            " unexcept step code : " + std::to_string(topProduct->getStep()));
    }
}

} // namespace grammer
} // namespace pangu