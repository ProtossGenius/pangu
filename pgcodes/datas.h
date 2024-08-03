#pragma once
#include "pipeline/assert.h"
#include "pipeline/datas.h"
#include "pipeline/pipeline.h"
#include <set>
#include <string>
namespace pangu {
namespace pgcodes {
typedef pglang::INameProduct IGrammer;
class GCode;
typedef std::unique_ptr<GCode> PCode;
enum class ValueType { NOT_VALUE = 0, IDENTIFIER, STRING, NUMBER };
class GCode : public IGrammer, public pglang::GStep {
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
    bool      isValue() {
        const static std::set<std::string> VALUE_SET{"(", "[", "++", "--"};
        return _value_type != ValueType::NOT_VALUE || VALUE_SET.count(_value);
    }

  private:
    std::string _value;
    ValueType   _value_type = ValueType::NOT_VALUE;
    PCode       _left;
    PCode       _right;
};
} // namespace pgcodes
} // namespace pangu