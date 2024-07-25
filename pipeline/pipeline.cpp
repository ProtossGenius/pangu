#include "pipeline/pipeline.h"
#include "pipeline/assert.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
#include <functional>
#include <iostream>
#include <ostream>
#include <stack>
#include <utility>
namespace pglang {
IPipelineFactory::IPipelineFactory(
    const std::string &name, PSwitcher &&switcher,
    std::map<int, std::function<PipelinePtr()>> &_pipelines,
    std::map<int, std::string>                  &pipeline_name_map,
    //  PPartsDealer          &&partsDealer,
    ProductPack finalProductPacker)
    : _name(name)
    , _switcher(std::move(switcher))
    , _pipelines(_pipelines)
    , _pipeline_name_map(pipeline_name_map)
    , _final_product_packer(finalProductPacker) {
    _switcher->_factory = this;
}
IPipeline *IPipelineFactory::getPipeline() {

    return _need_choise_pipeline || _pipeline_stack.empty() ||
                   _pipeline_stack.size() < _packer_stack.size()
               ? nullptr
               : _pipeline_stack.top().get();
}
void IPipelineFactory::pushProduct(PProduct &&pro, ProductPack pack) {
#ifdef DEBUG_MODE
    std::cout << name() << " pushProduct " << typeid(pro.get()).name()
              << "product = " << pro->to_string() << std::endl;
#endif
    pgassert_msg(_product_stack.size() <= _stack_max_size,
                 name() +
                     " when push product, product stack size is too large.");
    pgassert(pack != nullptr);
    pgassert(_product_stack.size() < _pipeline_stack.size());
    pgassert(_product_stack.size() == _packer_stack.size());
    pgassert(pro.get() != nullptr);
    _product_stack.emplace(std::move(pro));
    _packer_stack.push(pack);
}
void IPipelineFactory::accept(PData &&data) {
    _switcher->accept(std::move(data));
}
void IPipelineFactory::pushProduct(PProduct &&pro) {
    pushProduct(std::move(pro), _final_product_packer);
}
// void IPipelineFactory::unchoicePipeline() { _pipeline_stack.pop(); }
void IPipelineFactory::undealData(PData &&data) {
    _switcher->pushToCache(std::move(data));
}
#ifdef DEBUG_MODE
void print_stack(std::stack<PipelinePtr>                      &stack,
                 std::function<void(const std::string &, int)> print,
                 int                                           deep) {
    if (stack.empty()) return;
    auto &&it = std::move(stack.top());
    stack.pop();
    print(it.name(), deep);
    print_stack(stack, print, deep - 1);
    stack.push(std::move(it));
}
void print_stack(std::stack<PProduct>                &stack,
                 std::function<void(PProduct &, int)> print, int deep) {
    if (stack.empty()) return;
    PProduct it = std::move(stack.top());
    stack.pop();
    print(it, deep);
    print_stack(stack, print, deep - 1);
    stack.push(std::move(it));
}

#endif
void IPipelineFactory::status(std::ostream &ss) {
    using std::endl;
    ss << name() << " status:" << endl;
    ss << "product stack: size = " << _product_stack.size() << endl;
#ifdef DEBUG_MODE
    print_stack(
        _product_stack,
        [ & ](PProduct &it, int deep) {
            ss << "product_stack<" << deep << ">:" << it->to_string() << endl;
        },
        _product_stack.size());
#else
    if (!_product_stack.empty()) {
        ss << " top product is:" << _product_stack.top()->to_string() << endl;
    }
#endif
    ss << "pipeline stack: size = " << _pipeline_stack.size() << endl;
#ifdef DEBUG_MODE
    print_stack(
        _pipeline_stack,
        [ & ](const std::string &name, int deep) {
            ss << "pipe_stack<" << deep << ">:" << name << endl;
        },
        _pipeline_stack.size());
#else
    ss << "\t" << _pipeline_stack.top().name() << endl;
#endif
    ss << "packer_stack.size = " << _packer_stack.size() << endl;
}
IPipelineFactory::~IPipelineFactory() {
    if (!_pipeline_stack.empty()) {
        std::cerr << "error: " << name() << " _pipeline_stack not empty."
                  << std::endl;
        status(std::cout);
    }
}
void IPipelineFactory::packProduct() {
    PProduct pro = std::move(_product_stack.top());
#ifdef DEBUG_MODE
    std::cout << name() << " packProduct " << pro->to_string() << std::endl;
#endif
    _product_stack.pop();
    ProductPack pack = _packer_stack.top();
    _packer_stack.pop();
    pack(this, std::move(pro));
    _pipeline_stack.pop();
}

void IPipelineFactory::onFail(const std::string &errMsg) {
    _switcher->onFail(errMsg);
}

} // namespace pglang
