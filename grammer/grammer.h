#pragma once
#include "pipeline/declare.h"
#include "pgcodes/datas.h"
#include <memory>
namespace pangu {
namespace grammer {
pglang::PPipelineFactory create(pglang::ProductPack packer);

extern const pglang::ProductPack PACK_PRINT;
pglang::ProductPack              packNext(pglang::IPipelineFactory *factory);

// Parse an expression string into a GCode AST.
// Returns nullptr on failure.
std::unique_ptr<pgcodes::GCode> parseExpression(const std::string &expr_str);
} // namespace grammer
} // namespace pangu