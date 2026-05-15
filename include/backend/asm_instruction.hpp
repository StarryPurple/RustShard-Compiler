#ifndef RUST_SHARD_ASM_INSTRUCTION_H
#define RUST_SHARD_ASM_INSTRUCTION_H

#include "common/ir_instruction.hpp"

namespace rshard {

// 物理寄存器
enum class PhysReg {
  RAX, RBX, RCX, RDX, RSI, RDI, RSP, RBP,
  R8, R9, R10, R11, R12, R13, R14, R15,
  INVALID
};

struct MachineOperand {
  enum Kind { PhysReg, Imm, StackSlot };
  Kind kind;
  std::int64_t value;   // reg id / imm val / stack offset
  int size;             // byte count
};

// 机器指令
struct MachineInst {
  std::string opcode;                       // "mov", "add", "jmp", "call", etc.
  std::vector<MachineOperand> operands;
};

struct MachineBlock {
  std::vector<MachineInst> instructions;
};

}

#endif // RUST_SHARD_ASM_INSTRUCTION_H