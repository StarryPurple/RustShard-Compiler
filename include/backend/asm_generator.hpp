#ifndef RUST_SHARD_ASM_GENERATOR_HPP
#define RUST_SHARD_ASM_GENERATOR_HPP

#include "ir/ir_pack.hpp"
#include "backend/asm_pack.hpp"
#include "backend/regalloc.hpp"

namespace rshard::backend {

class AsmGenerator {
public:
  explicit AsmGenerator(const ir::IrPack& ir_pack);
  AsmPack generate();

private:
  AsmFunction generate_func(const ir::FunctionPack& func);

  void generate_prologue(const AllocationResult& alloc);
  void generate_epilogue(const AllocationResult& alloc);
  void generate_block(const ir::BasicBlockPack& bb, const AllocationResult& alloc);
  void generate_instruction(const ir::Instruction* inst, AsmBasicBlock& bb,
                            const AllocationResult& alloc);

  const ir::IrPack& _ir_pack;
  AsmFunction _asm_func;
};

} // namespace rshard::backend

#endif