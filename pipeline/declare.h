#pragma once
#include <functional>
#include <memory>
namespace pglang {
#define T(t)                                                                   \
    class I##t;                                                                \
    typedef std::unique_ptr<I##t> P##t;                                        \
    typedef std::shared_ptr<I##t> S##t;
// a factory, input is IData, output is IProduct.
// factory have many pipeline, use ISwitcher to decide witch pipeline deal
// IData.
T(PipelineFactory);

// input to factory.
T(Data);

// factory's output.
T(Product);

// when a product is finish, how deal with it.
// for example: for program-grammer, a class 'Big' nests another class 'Small'
// EXAMPLE: class Big{ ...; class Small { ... }; ... }
// for class Big, when it finish, it will as output give another factory.
// for class Small, when it finish, it should add into class Big.
typedef std::function<void(IPipelineFactory *, PProduct &&)> ProductPack;

// decide the IData should give witch Pipeline.
T(Switcher);

// deal IData, input is IData, output is IParts -- IProducts are made up of
// IParts
T(Pipeline);

// IPipeline's output.
T(Parts);

// add IParts into IProduct;
T(PartsDealer);

} // namespace pglang