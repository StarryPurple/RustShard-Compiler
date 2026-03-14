#ifndef RUST_SHARD_OPTIMIZER_H
#define RUST_SHARD_OPTIMIZER_H

#include "ir_pack.h"

namespace rshard::ir {
class Optimizer {
public:
  Optimizer() = default;
  virtual ~Optimizer() = default;

  virtual void optimize(IrPack &pack) = 0;
};

class Mem2RegOptimizer: public Optimizer {
public:
  void optimize(IrPack& pack) override {
    for(auto &fpack: pack.function_packs) optimize(fpack);
  }

private:
  void optimize(FunctionPack& pack) {

  }
};
}
