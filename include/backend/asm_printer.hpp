#ifndef RUST_SHARD_ASM_PRINTER_HPP
#define RUST_SHARD_ASM_PRINTER_HPP

#include <string>
#include "backend/asm_pack.hpp"

namespace rshard::backend {

struct AsmPrinter {
  static constexpr std::string INDENT = "\t";

  static std::string sprint(const AsmPack& pack);

private:
  static std::string sprint(const AsmFunction& func);
  static std::string sprint(const AsmBasicBlock& block);
  static std::string sprint(const AsmInstruction& inst);
  static std::string sprint(const AsmOperand& op);
  static std::string sprint_reg(PhysReg reg);
};

} // namespace rshard::backend

#endif