# Phase 11: Pipeline Syntax → Bootstrap

## Goal
Implement pipeline runtime syntax, evaluate bootstrap feasibility, fill gaps, complete self-hosting.

## Approach
1. Implement pipeline prerequisites (enum codegen, new operators)
2. Implement pipeline runtime lowering (state machine → LLVM IR)
3. Test with examples/bootstrap_lexer_pipeline.pgl
4. Evaluate: can pipeline syntax drive the bootstrap?
5. Fill remaining gaps, complete bootstrap compiler

## Tasks

### A: Pipeline Prerequisites
- [ ] A1: Enum codegen — `type X enum { A, B, C }` → integer constants 0,1,2; `X::A` resolves to 0
- [ ] A2: `=>` operator in lexer (arrow for case patterns)
- [ ] A3: break/continue in loops

### B: Pipeline Runtime Core
- [ ] B1: Parse impl worker/switcher methods into real function AST (not raw token capture)
- [ ] B2: Pipeline type → generate state machine struct + run function
- [ ] B3: Worker codegen — worker methods as state handler functions
- [ ] B4: Switcher codegen — dispatch function
- [ ] B5: Pipeline control signals — CONTINUE/FINISH/TRANSFER_FINISH/APPEND as builtins
- [ ] B6: Stream operator `>>` (push to output)

### C: Pattern Matching
- [ ] C1: `case(expr) { val => body }` syntax in parser
- [ ] C2: Case expression codegen (LLVM switch with return values)

### D: Integration & Test
- [ ] D1: Make examples/bootstrap_lexer_pipeline.pgl compile and run (pipeline version)
- [ ] D2: Evaluate pipeline bootstrap feasibility

### E: Bootstrap Completion
- [ ] E1: Fill any remaining syntax gaps identified in D2
- [ ] E2: Write bootstrap/emitter.pgl (AST → C code)
- [ ] E3: Write bootstrap/main.pgl (orchestrator)
- [ ] E4: Bootstrap verification — compile a simple PGL program end-to-end
