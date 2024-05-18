#pragma once

#include "pipeline/pipeline.h"
namespace pangu {
namespace lexer {
using namespace pglang;
enum ELexPipeline { Number = 2, Identifier, Space, Symbol, Comments, String };

class PipeNumber : IPipeline {};
class PipeIdentifier : IPipeline {};
class PipeSpace : IPipeline {};
class PipeSymbol : IPipeline {};
class PipeComments : IPipeline {};
class PipeString : IPipeline {};
} // namespace lexer
} // namespace pangu