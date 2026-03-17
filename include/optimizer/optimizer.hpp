#ifndef RUST_SHARD_OPTIMIZER_H
#define RUST_SHARD_OPTIMIZER_H

#include "common/ir_pack.hpp"

namespace rshard::ir {
class Optimizer {
public:
  Optimizer() = default;
  virtual ~Optimizer() = default;

  virtual void optimize(IrPack &pack) {
    for(auto &fpack: pack.function_packs) optimize(fpack);
  }
  virtual void optimize(FunctionPack& pack) {}
};

class Mem2RegOptimizer: public Optimizer {
public:
  void optimize(FunctionPack& pack) override {
    pack.update_block_ids();
    pack.construct_cfg();
  }
};
}
