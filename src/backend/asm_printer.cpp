#include "backend/asm_printer.hpp"

#include <format>

namespace rshard::backend {
std::string AsmPrinter::sprint(const AsmPack& pack) {
  std::string res;

  if(!pack.static_strings.empty()) {
    res += ".section .rodata\n";
    for(const auto& s: pack.static_strings) {
      res += s + "\n";
    }
    res += "\n";
  }

  res += ".section .text\n\n";
  for(size_t i = 0; i < pack.functions.size(); ++i) {
    if(i > 0) res += "\n";
    res += sprint(pack.functions[i]);
  }

  return res;
}

std::string AsmPrinter::sprint(const AsmFunction& func) {
  std::string res;

  res += ".globl " + func.name + "\n";
  res += func.name + ":\n";

  for(const auto& block: func.blocks) {
    res += sprint(block);
  }

  return res;
}

std::string AsmPrinter::sprint(const AsmBasicBlock& block) {
  std::string res;

  if(!block.label.empty()) {
    res += block.label + ":\n";
  }

  for(const auto& inst: block.instructions) {
    res += sprint(inst) + "\n";
  }

  return res;
}

std::string AsmPrinter::sprint(const AsmInstruction& inst) {
  if(inst.opcode.empty()) {
    if(inst.comment.empty()) return "";
    return "# " + inst.comment;
  }

  std::string res = INDENT + inst.opcode;

  for(size_t i = 0; i < inst.operands.size(); ++i) {
    res += (i == 0 ? " " : ", ");
    res += sprint(inst.operands[i]);
  }

  if(!inst.comment.empty()) {
    res = std::format("{:70}  # {}", res, inst.comment);
  }

  return res;
}

std::string AsmPrinter::sprint(const AsmOperand& op) {
  return std::visit([](const auto& v) -> std::string {
    using T = std::decay_t<decltype(v)>;
    if constexpr(std::is_same_v<T, AsmOperand::Reg>) {
      return sprint_reg(v.reg);
    } else if constexpr(std::is_same_v<T, AsmOperand::Imm>) {
      return std::to_string(v.val);
    } else if constexpr(std::is_same_v<T, AsmOperand::Label>) {
      return v.name;
    } else if constexpr(std::is_same_v<T, AsmOperand::Mem>) {
      return std::to_string(v.offset) + "(" + sprint_reg(v.base) + ")";
    }
    return "";
  }, op.value);
}

std::string AsmPrinter::sprint_reg(PhysReg reg) {
  auto it = kRegNames.find(reg);
  if(it != kRegNames.end()) return it->second;
  return "x" + std::to_string(static_cast<int>(reg));
}
} // namespace rshard::backend
