#ifndef RUST_SHARD_OPTIMIZER_H
#define RUST_SHARD_OPTIMIZER_H

#include "ir_pack.hpp"

namespace rshard::ir {

// optimization without modifying cfg.
// Include:
//   deadcode elimination
//   single-phi elimination
//   simple constant folding
//   struct type removal
class Canonicalization {
public:
  static void optimize(IrPack& ir) {
    for(auto &func: ir.function_packs) optimize(func);
  }
  static void optimize(FunctionPack& func);
};

class FunctionInline {
public:
  static void optimize(IrPack& ir);
};

// Or you can call it Mem2Reg.
class PromoteAlloca {
public:
  static void optimize(IrPack& ir) {
    for(auto &func: ir.function_packs) optimize(func);
    Canonicalization::optimize(ir);
  }
  static void optimize(FunctionPack& func);
};

// Strength reduction pass:
//   mul by power of 2  → shl
//   mul by small const → shift + add/sub chain
//   udiv by power of 2  → lshr
//   urem by power of 2  → and
class StrengthReduction {
public:
  static void optimize(IrPack& ir) {
    for(auto& func: ir.function_packs) optimize(func);
  }
  static void optimize(FunctionPack& func);
};

}

#endif // RUST_SHARD_OPTIMIZER_H































