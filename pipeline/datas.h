#pragma once
#include <string>
namespace pglang {

class IData {
  public:
    virtual int         typeId() const = 0;
    virtual std::string to_string() { return ""; }
    virtual ~IData() {}
};

class IProduct : public IData {
  public:
    virtual int         typeId() const override = 0;
    virtual std::string to_string() override { return ""; }
    virtual ~IProduct() {}
};

class Ignore : public IProduct {
  public:
    int typeId() const override { return 0; }
};

class INameProduct : public pglang::IProduct {
  public:
    virtual std::string integrityTest() { return ""; }
    virtual int         typeId() const override = 0;
    virtual std::string to_string() override    = 0;
    virtual ~INameProduct() {}

  public:
    std::string name() { return _name; }
    void        setName(const std::string &name) { _name = name; }

  protected:
    std::string _name;
};
} // namespace pglang