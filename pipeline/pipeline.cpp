#include "pipeline/pipeline.h"
#include "pipeline/declare.h"
#include "pipeline/switcher.h"
#include <cstddef>
#include <functional>
#include <iostream>
#include <ostream>
#include <stack>
#include <utility>
namespace pglang {
IPipelineFactory::IPipelineFactory(
    const std::string &name, PSwitcher &&switcher,
    std::map<int, PPipeline>   &_pipelines,
    std::map<int, std::string> &pipeline_name_map,
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
    return _index_stack.empty() || _index_stack.size() < _packer_stack.size()
               ? nullptr
               : _pipelines[ _index_stack.top() ].get();
}
void IPipelineFactory::pushProduct(PProduct &&pro, ProductPack pack) {
#ifdef DEBUG_MODE
    std::cout << name() << " pushProduct " << pro->to_string() << std::endl;
#endif
    _product_stack.emplace(std::move(pro));
    _packer_stack.push(pack);
}
void IPipelineFactory::accept(PData &&data) {
    _switcher->accept(std::move(data));
}
void IPipelineFactory::pushProduct(PProduct &&pro) {
    pushProduct(std::move(pro), _final_product_packer);
}
// void IPipelineFactory::unchoicePipeline() { _index_stack.pop(); }
void IPipelineFactory::undealData(PData &&data) {
    _switcher->pushToCache(std::move(data));
}
#ifdef DEBUG_MODE
void print_stack(std::stack<size_t>                &stack,
                 std::function<void(size_t &, int)> print, int deep) {
    if (stack.empty()) return;
    size_t it = stack.top();
    stack.pop();
    print(it, deep);
    print_stack(stack, print, deep - 1);
    stack.push(it);
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
    ss << "pipeline stack: size = " << _index_stack.size() << endl;
#ifdef DEBUG_MODE
    print_stack(
        _index_stack,
        [ & ](size_t it, int deep) {
            ss << "pipe_stack<" << deep << ">:" << _pipeline_name_map[ it ]
               << endl;
        },
        _index_stack.size());
#else
    ss << "\t" << _pipeline_name_map[ _index_stack.top() ] << endl;
#endif
}
IPipelineFactory::~IPipelineFactory() {
    if (!_index_stack.empty()) {
        std::cerr << "error: " << name() << " _index_stack not empty."
                  << std::endl;
        status(std::cout);
    }
}
void IPipelineFactory::packProduct() {
    PProduct pro = std::move(_product_stack.top());
#ifdef DEBUG_MODE
    std::cout << name() << " packProduct" << pro->to_string() << std::endl;
#endif
    _product_stack.pop();
    ProductPack pack = _packer_stack.top();
    _packer_stack.pop();
    pack(this, std::move(pro));
    _index_stack.pop();
}

void IPipelineFactory::onFail(const std::string &errMsg) {
    _switcher->onFail(errMsg);
}

} // namespace pglang
