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

// Or you can call it Mem2Reg.
class PromoteAlloca {
public:
  static void optimize(IrPack& ir) {
    for(auto &func: ir.function_packs) optimize(func);
  }
  static void optimize(FunctionPack& func);
};


}

#endif // RUST_SHARD_OPTIMIZER_H































