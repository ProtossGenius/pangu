#pragma once
#include "grammer/declare.h"
#include "grammer/enums.h"
#include "pipeline/assert.h"
#include "pipeline/pipeline.h"
#include <cstddef>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
namespace pangu {
namespace grammer {

class GStructContainer {
  public:
    void addStruct(PStruct &&stru);
    virtual ~GStructContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _structs.size(); }

  protected:
    std::map<std::string, PStruct> _structs;
};
class GFunctionContainer {
  public:
    void addFunction(PFunction &&fun);
    virtual ~GFunctionContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }

  protected:
    std::map<std::string, PFunction> _functions;
};

class GTypeFunctContainer {
  public:
    void addFunction(PFuncDef &&fun);
    virtual ~GTypeFunctContainer() {}
    void   write_string(std::ostream &ss);
    size_t size() const { return _functions.size(); }

  protected:
    std::map<std::string, PFuncDef> _functions;
};
class IGrammer : public pglang::IProduct {
  public:
    virtual std::string integrityTest() { return ""; }
    virtual int         typeId() const override = 0;
    virtual std::string to_string() override    = 0;
    virtual ~IGrammer() {}

  public:
    std::string name() { return _name; }
    void        setName(const std::string &name) { _name = name; }

  protected:
    std::string _name;
};

class GStep {
  public:
    GStep()
        : _step(0) {}
    virtual ~GStep() {}
    int  getStep() { return _step; }
    void setStep(int step) { this->_step = step; }

  private:
    int _step;
};

class GVarDefContainer : public IGrammer, public GStep {
  public:
    void addVariable(PVarDef &&var);
    void write_string(std::ostream &ss, const std::string &splitStr);
    virtual std::string integrityTest() override { return ""; }
    virtual int         typeId() const override { return 0; }
    virtual std::string to_string() override {
        std::stringstream ss;
        ss << "[";
        write_string(ss, ",");
        ss << "]";
        return ss.str();
    }
    size_t size() const { return _vars.size(); }
    virtual ~GVarDefContainer() {}
    void swap(GVarDefContainer &rhs) {
        _vars.swap(rhs._vars);
        _no_type_vars.swap(rhs._no_type_vars);
    }

  protected:
    std::map<std::string, PVarDef> _vars;

  private:
    std::set<std::string> _no_type_vars;
};

// package package_name;
class GPackage : public IGrammer, public GStep {
  public:
    int      typeId() const override { return EGrammer::Package; }
    void     addImport(PImport &&imp);
    GImport *getImport(const std::string &name) {
        return _imports.count(name) ? _imports[ name ].get() : nullptr;
    }
    std::string to_string() override;

    GFunctionContainer  functions;
    GTypeFunctContainer function_defs;
    GStructContainer    structs;
    GVarDefContainer    vars;

  private:
    std::map<std::string, PImport> _imports;
};
// import "package path" [as alias_name];
class GImport : public IGrammer, public GStep {
  public:
    int                typeId() const override { return EGrammer::Import; }
    const std::string &getPackage() { return _package; }
    void               setPackage(const std::string &pkg) { _package = pkg; }
    void               setAlias(const std::string &name) { setName(name); }
    std::string        alias() { return name(); }
    std::string        to_string() override;

  protected:
    std::string _package;
};
class GType : public IGrammer {
  public:
    int                typeId() const override { return 0; }
    const std::string &getPackage() { return _package; }
    void               read(std::string &str) {
        _name.swap(_package);
        _name.swap(str);
    }

    void copyFrom(const GType &rhs) {
        _name    = rhs._name;
        _package = rhs._package;
    }
    bool empty() { return _name.empty(); }

    std::string to_string() override;

  private:
    std::string _package;
};

class GVarDef : public IGrammer, public GStep {
  public:
    GVarDef()
        : _type(PType(new GType())) {}
    int typeId() const override { return 0; }

  public:
    void setDetail(const std::string &detail) { _detail = detail; }
    const std::string &getDetail() { return _detail; }
    GType             *getType() { return _type.get(); }
    std::string        integrityTest() override;
    std::string        to_string() override;

  private:
    PType       _type;
    std::string _detail;
};
class GTypeDef : public IGrammer, public GStep {
  public:
    virtual int         typeId() const override { return 0; }
    virtual std::string to_string() override { return "typedef: " + name(); }
};
class GStruct : public GVarDefContainer {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override;
};
class GIgnore : public IGrammer {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override { return "GIgnore"; }
};

class GFuncDef : public IGrammer, public GStep {
  public:
    int                 typeId() const override { return 4; }
    std::string         sign() { return ""; }
    virtual std::string to_string() override;
    GVarDefContainer    params;
    GVarDefContainer    result;
};

class GFunction : public GFuncDef {
  protected:
    PCode code;
};

enum class ValueType { NOT_VALUE = 0, IDENTIFIER, STRING, NUMBER };
inline void assert_empty(const std::string &str) {
    if (!str.empty()) {
        throw std::runtime_error("assert empty fail: string `" + str +
                                 "` not empty.");
    }
}
class GCode : public IGrammer, public GStep {
  public:
    int         typeId() const override { return 0; }
    std::string to_string() override {
        std::stringstream ss;
        write_string(ss);
        return ss.str();
    }
    void write_string(std::ostream &os);

  public:
    void setValue(const std::string &val, ValueType type) {
        assert_empty(_value);
        pgassert(type != ValueType::NOT_VALUE);
        _value_type = type;
        _value      = val;
    }
    void setOper(const std::string &val) {
        std::cout << _value << std::endl;
        pgassert(_value_type == ValueType::NOT_VALUE);
        assert_empty(_value);

        _value = val;
    }

    void setLeft(GCode *left) {
        pgassert_msg(_left.get() == nullptr,
                     "left value = " + _right->to_string());
        _left.reset(left);
    }

    void setRight(GCode *right) {
        pgassert_msg(_right.get() == nullptr,
                     "right value = " + _right->to_string());
        _right.reset(right);
    }
    GCode      *getLeft() { return _left.get(); }
    GCode      *getRight() { return _right.get(); }
    GCode      *releaseLeft() { return _left.release(); }
    GCode      *releaseRight() { return _right.release(); }
    void        swapChild() { _left.swap(_right); }
    std::string getValue() {
        pgassert(_value_type != ValueType::NOT_VALUE);
        return _value;
    }
    std::string getOper() {
        pgassert(_value_type == ValueType::NOT_VALUE);
        return _value;
    }
    ValueType getValueType() { return _value_type; }
    bool      isOper() { return _value_type == ValueType::NOT_VALUE; }
    bool      isValue() {
        return _value_type != ValueType::NOT_VALUE || _value == "(" ||
               _value == "[";
    }

  private:
    std::string _value;
    ValueType   _value_type = ValueType::NOT_VALUE;
    PCode       _left;
    PCode       _right;
};

} // namespace grammer
} // namespace pangu