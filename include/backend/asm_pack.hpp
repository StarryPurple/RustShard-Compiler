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
  struct Immediate { int64_t val; bool operator==(const Immediate& o) const { return val == o.val; } };

  std::variant<Register, SpillSlot, Immediate> value;

  static Location make_reg(PhysReg reg) { return {Register{reg}}; }
  static Location make_spill(int32_t offset) { return {SpillSlot{offset}}; }
  static Location make_imm(int64_t val) { return {Immediate{val}}; }

  bool is_reg() const { return std::holds_alternative<Register>(value); }
  bool is_spill() const { return std::holds_alternative<SpillSlot>(value); }
  bool is_imm() const { return std::holds_alternative<Immediate>(value); }

  PhysReg as_reg() const { return std::get<Register>(value).reg; }
  int32_t as_spill_offset() const { return std::get<SpillSlot>(value).offset; }
  int64_t as_imm() const { return std::get<Immediate>(value).val; }

  bool operator==(const Location& other) const { return value == other.value; }
  bool operator!=(const Location& other) const { return !(*this == other); }
};

struct AllocationResult {
  std::unordered_map<ir::reg_id_t, Location> mapping;
  std::unordered_set<PhysReg> callee_saved_used;
  std::size_t spill_area_size;
  std::size_t total_frame_size;
};

struct AsmBasicBlock {
  std::string label;
  std::vector<AsmInstruction> instructions;
};

struct AsmFunction {
  static constexpr const char* kEpilogueLabel = "Epilogue";
  std::string name;
  std::vector<AsmBasicBlock> blocks;
  std::size_t stack_frame_size;
  std::vector<PhysReg> callee_saved_regs;
  AllocationResult allocation_result;
};

struct AsmPack {
  std::vector<std::string> static_strings;
  std::vector<AsmFunction> functions;
};

} // namespace rshard::backend

#endif