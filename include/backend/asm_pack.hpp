#ifndef RUST_SHARD_ASM_PACK_HPP
#define RUST_SHARD_ASM_PACK_HPP

#include <cstdint>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include "common/common.hpp"
#include "backend/asm_instruction.hpp"
#include "ir/ir_pack.hpp"

namespace rshard::backend {

struct Location {
  struct Register { PhysReg reg; bool operator==(const Register& o) const { return reg == o.reg; } };
  struct SpillSlot { int32_t offset; bool operator==(const SpillSlot& o) const { return offset == o.offset; } };
  struct StackAddr { int32_t addr; bool operator==(const StackAddr& o) const { return addr == o.addr; } };

  std::variant<Register, SpillSlot, StackAddr> value;

  static Location make_reg(PhysReg reg) { return {Register{reg}}; }
  static Location make_spill(int32_t offset) { return {SpillSlot{offset}}; }
  static Location make_addr(int32_t addr) { return {StackAddr{addr}}; }

  bool is_reg() const { return std::holds_alternative<Register>(value); }
  bool is_spill() const { return std::holds_alternative<SpillSlot>(value); }
  bool is_addr() const { return std::holds_alternative<StackAddr>(value); }

  PhysReg as_reg() const { return std::get<Register>(value).reg; }
  int32_t as_spill_offset() const { return std::get<SpillSlot>(value).offset; }
  int32_t as_addr() const { return std::get<StackAddr>(value).addr; }

  bool operator==(const Location& other) const { return value == other.value; }
  bool operator!=(const Location& other) const { return !(*this == other); }
};

struct AsmBasicBlock {
  std::string label;
  std::vector<AsmInstruction> instructions;
};

struct AsmFunction {
  std::string name;
  std::vector<AsmBasicBlock> blocks;
};

struct AsmPack {
  std::vector<std::string> static_strings;
  std::vector<AsmFunction> functions;
};

} // namespace rshard::backend

#endif