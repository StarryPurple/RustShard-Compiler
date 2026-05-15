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

class PromoteAllocas: public Optimizer {
public:
  void optimize(IrPack& pack) override {
    for(auto &fpack: pack.function_packs) optimize(fpack);
  }
  void optimize(FunctionPack& pack) override {
    std::vector<std::unique_ptr<Instruction>> allocas;
    for(auto &bpack: pack.basic_block_packs) {
      auto &instrs = bpack.instructions;
      auto point = std::stable_partition(instrs.begin(), instrs.end(),
      [](const std::unique_ptr<Instruction> &ptr) {
        return dynamic_cast<AllocaInst*>(ptr.get()) != nullptr;
      });
      allocas.insert(allocas.end(), std::make_move_iterator(instrs.begin()), std::make_move_iterator(point));
      instrs.erase(instrs.begin(), point);
    }
    auto &entry = pack.basic_block_packs[0].instructions;
    entry.insert(entry.begin(), std::make_move_iterator(allocas.begin()), std::make_move_iterator(allocas.end()));

  }
};

class Mem2Reg: public Optimizer {
public:
  void optimize(IrPack& pack) override {
    for(auto &fpack: pack.function_packs) optimize(fpack);
  }
  void optimize(FunctionPack& pack) override;

private:
  struct InstIndex {
    block_id_t block_id;
    int inst_id; // shall be updated when inserting PhiInsts at the front.
  };
  std::vector<InstIndex> promote_allocas_;
  std::unordered_map<InstIndex, InstIndex> phi_to_alloca;
  std::unordered_map<InstIndex, std::vector<InstIndex>> value_stacks;
};
}

#endif // RUST_SHARD_OPTIMIZER_H