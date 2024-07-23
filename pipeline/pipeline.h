#pragma once
#include "pipeline/declare.h"
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <stack>
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

class IPipeline {
  public:
    virtual void accept(IPipelineFactory *factory, PData &&data) = 0;
    // create default product.
    virtual void onSwitch(IPipelineFactory *) = 0;
    virtual bool isClean() { return true; }
    virtual ~IPipeline() {}
};

class PipelinePtr {
  public:
    PipelinePtr(const PipelinePtr &) = delete;
    PipelinePtr()                    = delete;
    PipelinePtr(PipelinePtr &&rhs) {
        _pipeline = rhs._pipeline;
        _destory  = rhs._destory;
    }
    PipelinePtr(
        IPipeline                       *p,
        std::function<void(IPipeline *)> destory = [](IPipeline *) {})
        : _pipeline(p)
        , _destory(destory) {}
    IPipeline *get() { return _pipeline; }
    ~PipelinePtr() {
        if (_destory) _destory(_pipeline);
    }
    std::string name() { return _name; }
    void        setName(const std::string &name) { _name = name; }

  private:
    IPipeline                       *_pipeline;
    std::function<void(IPipeline *)> _destory;
    std::string                      _name;
};

// dataflow:
// (IData)  --> ISwitcher --> IPipeline --> (IParts)
//          --> IPartsDealer  ++> IProduct .
class IPipelineFactory : public IPipeline {
    // init area.
  public:
    IPipelineFactory(
        const std::string &name, PSwitcher &&switcher,
        std::map<int, std::function<PipelinePtr()>> &pipelines,
        std::map<int, std::string>                  &pipeline_name_map,
        /*PPartsDealer &&partsDealer, */ ProductPack finalProductPacker);
    //    ISwitcher *getSwitcher() { return _switcher.get(); }
    IPipeline *getPipeline();
    void       choicePipeline(size_t index) {
#ifdef DEBUG_MODE
        std::cout << name() << " choicePipeline(" << _pipeline_name_map[ index ]
                  << ")" << std::endl;
#endif
        assert(index >= 0 && index < _pipelines.size());
        _need_choise_pipeline = false;
        _pipeline_stack.push(std::move(_pipelines[ index ]()));
        assert(_pipeline_stack.top().get()->isClean());
        _pipeline_stack.top().setName(_pipeline_name_map[ index ]);
    }
    //   void      unchoicePipeline();
    void      pushProduct(PProduct &&pro, ProductPack pack);
    void      pushProduct(PProduct &&pro);
    void      waitChoisePipeline() { _need_choise_pipeline = true; }
    IProduct *getTopProduct() {
        return _product_stack.empty() ? nullptr : _product_stack.top().get();
    }
    PProduct &getTopProductPtr() {
        assert(!_product_stack.empty());
        return _product_stack.top();
    }
    size_t             productStackSize() { return _product_stack.size(); }
    void               packProduct();
    void               onFail(const std::string &errMsg);
    void               undealData(PData &&data);
    const std::string &name() { return _name; }
    void               status(std::ostream &ss);
    bool needSwitch() { return _pipeline_stack.size() > _product_stack.size(); }
    bool isClean() override {
        return _product_stack.empty() && _pipeline_stack.empty() &&
               _packer_stack.empty();
    }

  public:
    void accept(IPipelineFactory *factory, PData &&data) override {
        accept(std::move(data));
    }
    // switch to this pipeline will call onSwitch.
    void onSwitch(IPipelineFactory *) override {}
    void accept(PData &&data);
    virtual ~IPipelineFactory();
    void setMaxStackSize(size_t size) { _stack_max_size = size; }

  protected:
    std::string                                  _name;
    PSwitcher                                    _switcher;
    std::map<int, std::function<PipelinePtr()>> &_pipelines;
    std::map<int, std::string>                  &_pipeline_name_map;
    ProductPack                                  _final_product_packer;
    std::stack<PProduct>                         _product_stack;
    std::stack<ProductPack>                      _packer_stack;
    std::stack<PipelinePtr>                      _pipeline_stack;
    bool                                         _need_choise_pipeline;
    size_t                                       _stack_max_size = 10000;
};

class Reg {
  public:
    Reg(std::function<void()> action) { action(); }
};

const static Reg __REGISTER_TERMINAL_FUNCS([]() { registerTerminalFuncs(); });

class PipelineGetter {
  public:
    PipelineGetter(PipelinePtr *p)
        : _pipeline(p) {}
    IPipeline *operator()() { return _pipeline->get(); }

  private:
    std::shared_ptr<PipelinePtr> _pipeline;
};

} // namespace pglang