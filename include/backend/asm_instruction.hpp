#ifndef RUST_SHARD_ASM_INSTRUCTION_H
#define RUST_SHARD_ASM_INSTRUCTION_H

#include <string>
#include <vector>
#include <variant>
#include "asm_defs.hpp"

namespace rshard::backend {
struct AsmOperand {
  struct Reg {
    PhysReg reg;
    bool operator==(const Reg& o) const { return reg == o.reg; }
  };

  struct Imm {
    int64_t val;
    bool operator==(const Imm& o) const { return val == o.val; }
  };

  struct Label {
    std::string name;
    bool operator==(const Label& o) const { return name == o.name; }
  };

  struct Mem {
    PhysReg base;
    int32_t offset;
    bool operator==(const Mem& o) const { return base == o.base && offset == o.offset; }
  };

  std::variant<Reg, Imm, Label, Mem> value;

  static AsmOperand reg(PhysReg r) { return {Reg{r}}; }
  static AsmOperand imm(int64_t v) { return {Imm{v}}; }
  static AsmOperand label(std::string n) { return {Label{std::move(n)}}; }
  static AsmOperand mem(PhysReg b, int32_t o) { return {Mem{b, o}}; }

  bool is_reg() const { return std::holds_alternative<Reg>(value); }
  bool is_imm() const { return std::holds_alternative<Imm>(value); }
  bool is_label() const { return std::holds_alternative<Label>(value); }
  bool is_mem() const { return std::holds_alternative<Mem>(value); }

  PhysReg as_reg() const { return std::get<Reg>(value).reg; }
  int64_t as_imm() const { return std::get<Imm>(value).val; }
  const std::string& as_label() const { return std::get<Label>(value).name; }
  const Mem& as_mem() const { return std::get<Mem>(value); }

  bool operator==(const AsmOperand& o) const { return value == o.value; }
  bool operator!=(const AsmOperand& o) const { return !(*this == o); }

  std::string to_string() const;
};

struct AsmInstruction {
  std::string opcode;
  std::vector<AsmOperand> operands;
  std::string comment;

  static AsmInstruction R(std::string op, PhysReg rd, PhysReg rs1, PhysReg rs2, std::string cmt = "");
  static AsmInstruction I(std::string op, PhysReg rd, PhysReg rs1, int64_t imm, std::string cmt = "");
  static AsmInstruction L(std::string op, PhysReg rd, PhysReg base, int32_t offset, std::string cmt = "");
  static AsmInstruction S(std::string op, PhysReg rs2, PhysReg base, int32_t offset, std::string cmt = "");
  static AsmInstruction B(std::string op, PhysReg rs1, PhysReg rs2, std::string label, std::string cmt = "");
  static AsmInstruction U(std::string op, PhysReg rd, int64_t imm, std::string cmt = "");
  static AsmInstruction J(std::string op, PhysReg rd, std::string label, std::string cmt = "");
  static AsmInstruction JI(std::string op, PhysReg rd, PhysReg rs1, int64_t offset, std::string cmt = "");
  static AsmInstruction O(std::string op, std::string cmt = "");
  static AsmInstruction P(std::string op, std::string label, std::string cmt = "");
  static AsmInstruction BL(std::string op, PhysReg rs, std::string label, std::string cmt = "");
  static AsmInstruction RR(std::string op, PhysReg rd, PhysReg rs, std::string cmt = "");
  static AsmInstruction RI(std::string op, PhysReg rd, int64_t imm, std::string cmt = "");
};

namespace RV64I {
  inline AsmInstruction ADD(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("add", rd, rs1, rs2, c);
  }

  inline AsmInstruction ADDI(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("addi", rd, rs1, imm, c);
  }

  inline AsmInstruction SUB(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("sub", rd, rs1, rs2, c);
  }

  inline AsmInstruction MUL(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("mul", rd, rs1, rs2, c);
  }

  inline AsmInstruction DIV(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("div", rd, rs1, rs2, c);
  }

  inline AsmInstruction REM(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("rem", rd, rs1, rs2, c);
  }

  inline AsmInstruction AND(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("and", rd, rs1, rs2, c);
  }

  inline AsmInstruction ANDI(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("andi", rd, rs1, imm, c);
  }

  inline AsmInstruction OR(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("or", rd, rs1, rs2, c);
  }

  inline AsmInstruction ORI(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("ori", rd, rs1, imm, c);
  }

  inline AsmInstruction XOR(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("xor", rd, rs1, rs2, c);
  }

  inline AsmInstruction XORI(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("xori", rd, rs1, imm, c);
  }

  inline AsmInstruction SLL(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("sll", rd, rs1, rs2, c);
  }

  inline AsmInstruction SLLI(PhysReg rd, PhysReg rs1, int64_t sh, std::string c = "") {
    return AsmInstruction::I("slli", rd, rs1, sh, c);
  }

  inline AsmInstruction SRL(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("srl", rd, rs1, rs2, c);
  }

  inline AsmInstruction SRLI(PhysReg rd, PhysReg rs1, int64_t sh, std::string c = "") {
    return AsmInstruction::I("srli", rd, rs1, sh, c);
  }

  inline AsmInstruction SRA(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("sra", rd, rs1, rs2, c);
  }

  inline AsmInstruction SRAI(PhysReg rd, PhysReg rs1, int64_t sh, std::string c = "") {
    return AsmInstruction::I("srai", rd, rs1, sh, c);
  }

  inline AsmInstruction SLT(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("slt", rd, rs1, rs2, c);
  }

  inline AsmInstruction SLTI(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("slti", rd, rs1, imm, c);
  }

  inline AsmInstruction SLTU(PhysReg rd, PhysReg rs1, PhysReg rs2, std::string c = "") {
    return AsmInstruction::R("sltu", rd, rs1, rs2, c);
  }

  inline AsmInstruction SLTIU(PhysReg rd, PhysReg rs1, int64_t imm, std::string c = "") {
    return AsmInstruction::I("sltiu", rd, rs1, imm, c);
  }

  inline AsmInstruction LB(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lb", rd, b, off, c);
  }

  inline AsmInstruction LH(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lh", rd, b, off, c);
  }

  inline AsmInstruction LW(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lw", rd, b, off, c);
  }

  inline AsmInstruction LD(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("ld", rd, b, off, c);
  }

  inline AsmInstruction LBU(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lbu", rd, b, off, c);
  }

  inline AsmInstruction LHU(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lhu", rd, b, off, c);
  }

  inline AsmInstruction LWU(PhysReg rd, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::L("lwu", rd, b, off, c);
  }

  inline AsmInstruction SB(PhysReg rs, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::S("sb", rs, b, off, c);
  }

  inline AsmInstruction SH(PhysReg rs, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::S("sh", rs, b, off, c);
  }

  inline AsmInstruction SW(PhysReg rs, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::S("sw", rs, b, off, c);
  }

  inline AsmInstruction SD(PhysReg rs, PhysReg b, int32_t off, std::string c = "") {
    return AsmInstruction::S("sd", rs, b, off, c);
  }

  inline AsmInstruction BEQ(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("beq", a, b, std::move(l), c);
  }

  inline AsmInstruction BNE(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("bne", a, b, std::move(l), c);
  }

  inline AsmInstruction BLT(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("blt", a, b, std::move(l), c);
  }

  inline AsmInstruction BGE(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("bge", a, b, std::move(l), c);
  }

  inline AsmInstruction BLTU(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("bltu", a, b, std::move(l), c);
  }

  inline AsmInstruction BGEU(PhysReg a, PhysReg b, std::string l, std::string c = "") {
    return AsmInstruction::B("bgeu", a, b, std::move(l), c);
  }

  inline AsmInstruction JAL(PhysReg rd, std::string l, std::string c = "") {
    return AsmInstruction::J("jal", rd, std::move(l), c);
  }

  inline AsmInstruction JALR(PhysReg rd, PhysReg rs, int64_t off, std::string c = "") {
    return AsmInstruction::JI("jalr", rd, rs, off, c);
  }

  inline AsmInstruction LUI(PhysReg rd, int64_t imm, std::string c = "") {
    return AsmInstruction::U("lui", rd, imm, c);
  }

  inline AsmInstruction AUIPC(PhysReg rd, int64_t imm, std::string c = "") {
    return AsmInstruction::U("auipc", rd, imm, c);
  }

  inline AsmInstruction MV(PhysReg rd, PhysReg rs, std::string c = "") { return AsmInstruction::RR("mv", rd, rs, c); }
  inline AsmInstruction LI(PhysReg rd, int64_t imm, std::string c = "") { return AsmInstruction::RI("li", rd, imm, c); }
  inline AsmInstruction NOT(PhysReg rd, PhysReg rs, std::string c = "") { return AsmInstruction::RR("not", rd, rs, c); }
  inline AsmInstruction NEG(PhysReg rd, PhysReg rs, std::string c = "") { return AsmInstruction::RR("neg", rd, rs, c); }

  inline AsmInstruction SEQZ(PhysReg rd, PhysReg rs, std::string c = "") {
    return AsmInstruction::RR("seqz", rd, rs, c);
  }

  inline AsmInstruction SNEZ(PhysReg rd, PhysReg rs, std::string c = "") {
    return AsmInstruction::RR("snez", rd, rs, c);
  }

  inline AsmInstruction J(std::string l, std::string c = "") { return AsmInstruction::P("j", std::move(l), c); }
  inline AsmInstruction CALL(std::string f, std::string c = "") { return AsmInstruction::P("call", std::move(f), c); }
  inline AsmInstruction RET(std::string c = "") { return AsmInstruction::O("ret", c); }
  inline AsmInstruction NOP(std::string c = "") { return AsmInstruction::O("nop", c); }

  inline AsmInstruction BEQZ(PhysReg rs, std::string l, std::string c = "") {
    return AsmInstruction::BL("beqz", rs, std::move(l), c);
  }

  inline AsmInstruction BNEZ(PhysReg rs, std::string l, std::string c = "") {
    return AsmInstruction::BL("bnez", rs, std::move(l), c);
  }

  inline AsmInstruction ECALL(std::string c = "") { return AsmInstruction::O("ecall", c); }
}
} // namespace rshard::backend

#endif // RUST_SHARD_ASM_INSTRUCTION_H