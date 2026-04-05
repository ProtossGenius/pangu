#pragma once
#include "pipeline/declare.h"
namespace pangu {
namespace lexer {
pglang::PPipelineFactory create(pglang::ProductPack packer);
void analysis(const std::string &file, pglang::ProductPack packer);
void analysis(const std::string &file, pglang::PPipelineFactory factory);
void analysisString(const std::string &source, pglang::PPipelineFactory factory);

extern const pglang::ProductPack PACK_PRINT;
pglang::ProductPack              packNext(pglang::IPipelineFactory *factory);
} // namespace lexer
} // namespace pangu