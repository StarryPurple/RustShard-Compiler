#include "backend/asm_generator.hpp"
#include "backend/asm_instruction.hpp"
#include <deque>
#include <algorithm>
#include <format>

namespace rshard::backend {
namespace {
  /************************************* Helper functions **********************************************/
  constexpr std::string kEpilogueLabel = "epilogue";
  constexpr std::string kFuncPrefix = ".F.";

  std::string mangle_func_name(const std::string& original) {
    static const std::unordered_set<std::string> reserved = {
        "printInt", "printlnInt", "getInt", "exit", "main", "memcpy"
      };
    if(reserved.contains(original)) return original;
    return kFuncPrefix + original;
  }

  std::string mangle_label(const std::string& func_name, const std::string& label) {
    return func_name + "." + label;
  }

  bool is_i12(int32_t offset) { return -2048 <= offset && offset <= 2047; }
  bool is_i12(int64_t offset) { return -2048 <= offset && offset <= 2047; }


  /*
  bool is_32bit(ir::IrType irtype) {
    auto prime = irtype.type().get_if<stype::PrimeType>();
    if(!prime) return false;
    return prime->size() <= 4;
  }

  AsmOperand loc_to_oper(const Location& loc) {
    if(loc.is_reg()) return AsmOperand::reg(loc.as_reg());
    if(loc.is_spill()) return AsmOperand::mem(PhysReg::sp, loc.as_spill());
    throw std::runtime_error("Unreachable loc_to_oper end");
  }

  AsmOperand ir_op_to_asm(const ir::Operand& op, const AllocationResult& alloc) {
    if(op.is_imm()) return AsmOperand::imm(op.value);
    return loc_to_oper(alloc.mapping.at(op.value));
  }
  */

  void try_sd(PhysReg reg, PhysReg base, int32_t offset, AsmBasicBlock& bb, PhysReg tmp = PhysReg::zero) {
    if(is_i12(offset)) {
      bb.instructions.push_back(RV64I::SD(reg, base, offset));
    } else {
      if(tmp == PhysReg::zero) throw std::runtime_error("try_sd not assigned with helper");
      bb.instructions.push_back(RV64I::LI(tmp, offset));
      bb.instructions.push_back(RV64I::ADD(tmp, base, tmp));
      bb.instructions.push_back(RV64I::SD(reg, tmp, 0));
    }
  }

  void try_ld(PhysReg reg, PhysReg base, int32_t offset, AsmBasicBlock& bb) {
    if(is_i12(offset)) {
      bb.instructions.push_back(RV64I::LD(reg, base, offset));
    } else {
      bb.instructions.push_back(RV64I::LI(reg, offset));
      bb.instructions.push_back(RV64I::ADD(reg, base, reg));
      bb.instructions.push_back(RV64I::LD(reg, reg, 0));
    }
  }

  void try_lea(PhysReg reg, int32_t offset, AsmBasicBlock& bb) {
    if(is_i12(offset)) {
      bb.instructions.push_back(RV64I::ADDI(reg, PhysReg::sp, offset));
    } else {
      bb.instructions.push_back(RV64I::LI(reg, offset));
      bb.instructions.push_back(RV64I::ADD(reg, PhysReg::sp, reg));
    }
  }

  void try_addi(PhysReg rd, PhysReg rs1, int32_t imm, AsmBasicBlock& bb, PhysReg tmp = PhysReg::zero) {
    if(imm >= -2048 && imm <= 2047) {
      if(imm == 0 && rd == rs1) return;
      bb.instructions.push_back(RV64I::ADDI(rd, rs1, imm));
    } else if(rd == rs1) {
      if(tmp == PhysReg::zero) throw std::runtime_error("try_addi not assigned with helper");
      bb.instructions.push_back(RV64I::LI(tmp, imm));
      bb.instructions.push_back(RV64I::ADD(rd, rs1, tmp));
    } else {
      bb.instructions.push_back(RV64I::LI(rd, imm));
      bb.instructions.push_back(RV64I::ADD(rd, rs1, rd));
    }
  }

  PhysReg oper_phys_dst(ir::reg_id_t vreg, const AllocationResult& alloc) {
    const auto& loc = alloc.mapping.at(vreg);
    return loc.is_reg() ? loc.as_reg() : kTmpRd;
  }

  PhysReg oper_phys_src_rs1(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
    const auto& loc = alloc.mapping.at(vreg);
    if(loc.is_reg()) return loc.as_reg();
    if(loc.is_addr()) {
      try_lea(kTmpRs1, loc.as_addr(), bb);
      return kTmpRs1;
    }
    // bb.instructions.push_back(RV64I::LD(kTmpRs1, PhysReg::sp, loc.as_spill()));
    try_ld(kTmpRs1, PhysReg::sp, loc.as_spill(), bb);
    return kTmpRs1;
  }

  PhysReg oper_phys_src_rs2(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
    const auto& loc = alloc.mapping.at(vreg);
    if(loc.is_reg()) return loc.as_reg();
    if(loc.is_addr()) {
      bb.instructions.push_back(RV64I::LineComment(std::format("alloca %{}", vreg)));
      try_lea(kTmpRs2, loc.as_addr(), bb);
      return kTmpRs2;
    }
    // bb.instructions.push_back(RV64I::LD(kTmpRs2, PhysReg::sp, loc.as_spill()));
    try_ld(kTmpRs2, PhysReg::sp, loc.as_spill(), bb);
    return kTmpRs2;
  }

  // since kTmpRd is still in use, you shall only pass kTmpRs1/kTmpRs2
  void emit_spill_store_if_needed(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb, PhysReg tmp) {
    const auto& loc = alloc.mapping.at(vreg);
    if(loc.is_spill()) {
      try_sd(kTmpRd, PhysReg::sp, loc.as_spill(), bb, tmp);
      // bb.instructions.push_back(RV64I::SD(kTmpRd, PhysReg::sp, loc.as_spill()));
    }
  }

  // kTmpRs1 and kTmpRs2 are used.
  void location_assign(Location dst_loc, Location src_loc, AsmBasicBlock& bb) {
    // dst shall be reg/spill; src can be imm/reg/addr/spill.
    if(src_loc.is_imm()) {
      auto imm = src_loc.as_imm();
      if(dst_loc.is_reg()) {
        bb.instructions.push_back(RV64I::LI(dst_loc.as_reg(), imm));
      } else if(dst_loc.is_spill()) {
        bb.instructions.push_back(RV64I::LI(kTmpRs2, imm));
        try_sd(kTmpRs2, PhysReg::sp, dst_loc.as_spill(), bb, kTmpRs1);
      } else {
        throw std::runtime_error("location assign: invalid dst_loc type");
      }
    } else if(src_loc.is_reg()) {
      auto preg = src_loc.as_reg();
      if(dst_loc.is_reg()) {
        if(dst_loc.as_reg() != preg)
          bb.instructions.push_back(RV64I::MV(dst_loc.as_reg(), preg));
      } else if(dst_loc.is_spill()) {
        bb.instructions.push_back(RV64I::MV(kTmpRs2, preg));
        try_sd(kTmpRs2, PhysReg::sp, dst_loc.as_spill(), bb, kTmpRs1);
      } else {
        throw std::runtime_error("location assign: invalid dst_loc type");
      }
    } else if(src_loc.is_addr()) {
      try_lea(kTmpRs1, src_loc.as_addr(), bb);
      if(dst_loc.is_reg()) {
        bb.instructions.push_back(RV64I::MV(dst_loc.as_reg(), kTmpRs1));
      } else if(dst_loc.is_spill()) {
        bb.instructions.push_back(RV64I::MV(kTmpRs2, kTmpRs1));
        try_sd(kTmpRs2, PhysReg::sp, dst_loc.as_spill(), bb, kTmpRs1);
      } else {
        throw std::runtime_error("location assign: invalid dst_loc type");
      }
    } else if(src_loc.is_spill()) {
      try_ld(kTmpRs1, PhysReg::sp, src_loc.as_spill(), bb);
      if(dst_loc.is_reg()) {
        bb.instructions.push_back(RV64I::MV(dst_loc.as_reg(), kTmpRs1));
      } else if(dst_loc.is_spill()) {
        bb.instructions.push_back(RV64I::MV(kTmpRs2, kTmpRs1));
        try_sd(kTmpRs2, PhysReg::sp, dst_loc.as_spill(), bb, kTmpRs1);
      } else {
        throw std::runtime_error("location assign: invalid dst_loc type");
      }
    }
  }

  void
  parallel_assignment(std::unordered_map<Location, Location, Location::Hash>&& dst_to_src, AsmBasicBlock& bb) {
    // dst shall be reg/spill; src can be imm/reg/addr/spill.
    // All nodes have only one in-degree (only be dst for at most one time).
    // That's why we use unordered_map to store them.
    std::vector<AsmInstruction> instructions;
    // remove all nodes without out-degree (only be dst, not src) recursively.
    while(!dst_to_src.empty()) {
      bool removed = false;
      for(auto it = dst_to_src.begin(); it != dst_to_src.end();) {
        auto [dst_loc, src_loc] = *it;
        bool dst_safe = true;
        for(const auto& [dst_loc2, src_loc2]: dst_to_src) {
          if(src_loc2 == dst_loc) {
            // dst value still needed
            dst_safe = false;
            break;
          }
        }
        if(dst_safe) {
          // safe assignment
          // This is safe. See cppreference.
          location_assign(dst_loc, src_loc, bb);
          it = dst_to_src.erase(it);
          removed = true;
        } else {
          ++it;
        }
      }
      if(!removed) break;
    }
    // Then, what are left must be cycles only.
    while(!dst_to_src.empty()) {
      Location dst_loc = dst_to_src.begin()->first;
      Location src_loc = dst_to_src.begin()->second;
      if(src_loc == dst_loc) {
        // self loop.
        dst_to_src.erase(dst_loc);
        continue;
      }
      Location backup = dst_loc;
      // move backup into kTmpRd.
      location_assign(Location::make_reg(kTmpRd), backup, bb);
      while(src_loc != backup) {
        location_assign(dst_loc, src_loc, bb);
        dst_to_src.erase(dst_loc);
        dst_loc = src_loc;
        src_loc = dst_to_src.at(src_loc);
      }
      location_assign(dst_loc, Location::make_reg(kTmpRd), bb);
      dst_to_src.erase(dst_loc);
    }
  }

  /************************************* Magic Number Div/Rem **********************************************/

  struct MagicU64 { uint64_t magic; int shift; };
  MagicU64 compute_magic_u64(uint64_t d) {
    for(int p = 0; p < 64; ++p) {
      __uint128_t pow = static_cast<__uint128_t>(1) << (64 + p);
      __uint128_t M_128 = pow / d + 1;
      if(M_128 >= static_cast<__uint128_t>(1) << 64) continue;
      uint64_t M = static_cast<uint64_t>(M_128);
      if(static_cast<uint64_t>(pow % d) <= (1ULL << p))
        return {M, p};
    }
    return {0, 0};
  }

   bool try_const_udiv_urem(const std::string& op, uint64_t divisor,
                             PhysReg dst, PhysReg src, AsmBasicBlock& bb) {
    return false; // disable
    if(divisor <= 1 || (divisor & (divisor - 1)) == 0) return false;
    auto [M, s] = compute_magic_u64(divisor);
    if(M == 0) return false;
    bb.instructions.push_back(RV64I::LI(kTmpRs2, static_cast<int64_t>(M)));
    bb.instructions.push_back(RV64I::MULHU(kTmpRd, src, kTmpRs2));
    if(op == "urem") {
      if(s > 0) bb.instructions.push_back(RV64I::SRLI(kTmpRd, kTmpRd, s));
      bb.instructions.push_back(RV64I::LI(kTmpRs2, static_cast<int64_t>(divisor)));
      bb.instructions.push_back(RV64I::MUL(kTmpRd, kTmpRd, kTmpRs2));
      bb.instructions.push_back(RV64I::SUB(dst, src, kTmpRd));
    } else {
      if(s > 0) bb.instructions.push_back(RV64I::SRLI(dst, kTmpRd, s));
      else if(dst != kTmpRd) bb.instructions.push_back(RV64I::MV(dst, kTmpRd));
    }
    return true;
  }

  // Signed magic number (Granlund-Montgomery, via Hacker's Delight / libdivide).
  struct MagicS64 { int64_t multiplier; int shift; };
  MagicS64 compute_magic_s64(int64_t d) {
    uint64_t abs_d = d < 0 ? -static_cast<uint64_t>(d) : static_cast<uint64_t>(d);
    __uint128_t two_63 = (__uint128_t)1 << 63;
    __uint128_t tmp = two_63 + (d < 0 ? 1 : 0);
    __uint128_t anc = tmp - 1 - tmp % abs_d;
    __uint128_t p = 63;
    __uint128_t q1 = two_63 / anc, r1 = two_63 % anc;
    __uint128_t q2 = two_63 / abs_d, r2 = two_63 % abs_d;
    do {
      p++;
      q1 <<= 1; r1 <<= 1;
      if(r1 >= anc) { q1++; r1 -= anc; }
      q2 <<= 1; r2 <<= 1;
      if(r2 >= abs_d) { q2++; r2 -= abs_d; }
    } while(q1 < abs_d - r2 || (q1 == abs_d - r2 && r1 == 0));
    MagicS64 res;
    res.multiplier = static_cast<int64_t>(q2 + 1);
    if(d < 0) res.multiplier = -res.multiplier;
    res.shift = static_cast<int>(p - 64);
    return res;
  }

  bool try_const_sdiv_srem(const std::string& op, int64_t divisor,
                             PhysReg dst, PhysReg src, AsmBasicBlock& bb) {
    return false; // disable
    if(divisor == 0 || divisor == 1 || divisor == -1) return false;
    uint64_t d_abs = divisor < 0 ? -static_cast<uint64_t>(divisor) : static_cast<uint64_t>(divisor);

    // Signed division by power of 2: use srai + correction
    if((d_abs & (d_abs - 1)) == 0) {
      int n = __builtin_ctzll(d_abs);
      if(n == 0) return false;
      if(op == "sdiv") {
        bb.instructions.push_back(RV64I::SRAI(kTmpRs2, src, 63));
        if(n < 64) bb.instructions.push_back(RV64I::SRLI(kTmpRs2, kTmpRs2, 64 - n));
        bb.instructions.push_back(RV64I::ADD(kTmpRd, src, kTmpRs2));
        bb.instructions.push_back(RV64I::SRAI(dst, kTmpRd, n));
        if(divisor < 0 && dst != PhysReg::zero)
          bb.instructions.push_back(RV64I::NEG(dst, dst));
        return true;
      }
      if(op == "srem") {
        bb.instructions.push_back(RV64I::SRAI(kTmpRs2, src, 63));
        if(n < 64) bb.instructions.push_back(RV64I::SRLI(kTmpRs2, kTmpRs2, 64 - n));
        bb.instructions.push_back(RV64I::ADD(kTmpRd, src, kTmpRs2));
        bb.instructions.push_back(RV64I::SRAI(kTmpRd, kTmpRd, n));
        bb.instructions.push_back(RV64I::SLLI(kTmpRd, kTmpRd, n));
        bb.instructions.push_back(RV64I::SUB(dst, src, kTmpRd));
        return true;
      }
      return false;
    }

    // General signed division by constant: magic number (Granlund-Montgomery)
    // Reference: LLVM lib/Support/DivisionByConstantInfo.cpp, TargetLowering.cpp BuildSDIV.
    // Sequence: mulh → add/sub numerator → sra → srli correction → add correction
    auto magic = compute_magic_s64(divisor);
    bb.instructions.push_back(RV64I::LI(kTmpRs2, magic.multiplier));
    bb.instructions.push_back(RV64I::MULH(kTmpRd, src, kTmpRs2));

    // t0 = mulh(src, M); optionally add/subtract src as correction
    if(divisor > 0 && magic.multiplier < 0)
      bb.instructions.push_back(RV64I::ADD(kTmpRd, kTmpRd, src));
    else if(divisor < 0 && magic.multiplier > 0)
      bb.instructions.push_back(RV64I::SUB(kTmpRd, kTmpRd, src));

    // Post-shift
    if(magic.shift > 0)
      bb.instructions.push_back(RV64I::SRAI(kTmpRd, kTmpRd, magic.shift));

    // Correction for truncation towards zero:
    //   result += (result < 0)
    bb.instructions.push_back(RV64I::SRLI(kTmpRs2, kTmpRd, 63));
    bb.instructions.push_back(RV64I::ADD(kTmpRd, kTmpRd, kTmpRs2));

    if(op == "srem") {
      // r = n - q * d
      bb.instructions.push_back(RV64I::LI(kTmpRs2, divisor));
      bb.instructions.push_back(RV64I::MUL(kTmpRd, kTmpRd, kTmpRs2));
      bb.instructions.push_back(RV64I::SUB(dst, src, kTmpRd));
    } else {
      if(dst != kTmpRd) bb.instructions.push_back(RV64I::MV(dst, kTmpRd));
    }
    return true;
  }

  /************************************* IrInstr -> AsmInstr **********************************************/

  void generate_binary_op(const ir::BinaryOpInst& bin, const AllocationResult& alloc,
                          AsmBasicBlock& bb) {
    PhysReg lhs = bin.lhs.is_imm() ? PhysReg::zero : oper_phys_src_rs1(bin.lhs.as_reg(), alloc, bb);
    PhysReg rhs = bin.rhs.is_imm() ? PhysReg::zero : oper_phys_src_rs2(bin.rhs.as_reg(), alloc, bb);
    PhysReg dst = oper_phys_dst(bin.dst, alloc);

    if(bin.lhs.is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs1, bin.lhs.as_imm()));
      lhs = kTmpRs1;
    }
    if(bin.rhs.is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs2, bin.rhs.as_imm()));
      rhs = kTmpRs2;
    }

    const std::string& op = bin.op;

    if(op == "add") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::ADDI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::ADDI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::ADD(dst, lhs, rhs));
      }
    } else if(op == "sub") {
      if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(-bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::ADDI(dst, lhs, -bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::SUB(dst, lhs, rhs));
      }
    } else if(op == "mul") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && bin.lhs.as_imm() > 0 && (bin.lhs.as_imm() & (bin.lhs.as_imm() - 1)) == 0) {
        bb.instructions.back() = RV64I::SLLI(dst, rhs, __builtin_ctzll(bin.lhs.as_imm()));
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && bin.rhs.as_imm() > 0 && (bin.rhs.as_imm() & (bin.rhs.as_imm() - 1)) == 0) {
        bb.instructions.back() = RV64I::SLLI(dst, lhs, __builtin_ctzll(bin.rhs.as_imm()));
      } else {
        bb.instructions.push_back(RV64I::MUL(dst, lhs, rhs));
      }
    } else if(op == "udiv") {
      if(!bin.rhs.is_imm() || !try_const_udiv_urem("udiv", bin.rhs.as_imm(), dst, lhs, bb))
        bb.instructions.push_back(RV64I::DIVU(dst, lhs, rhs));
    } else if(op == "urem") {
      if(!bin.rhs.is_imm() || !try_const_udiv_urem("urem", bin.rhs.as_imm(), dst, lhs, bb))
        bb.instructions.push_back(RV64I::REMU(dst, lhs, rhs));
    } else if(op == "sdiv") {
      if(!bin.rhs.is_imm() || !try_const_sdiv_srem("sdiv", bin.rhs.as_imm(), dst, lhs, bb))
        bb.instructions.push_back(RV64I::DIV(dst, lhs, rhs));
    } else if(op == "srem") {
      if(!bin.rhs.is_imm() || !try_const_sdiv_srem("srem", bin.rhs.as_imm(), dst, lhs, bb))
        bb.instructions.push_back(RV64I::REM(dst, lhs, rhs));
    } else if(op == "and") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::ANDI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::ANDI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::AND(dst, lhs, rhs));
      }
    } else if(op == "or") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::ORI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::ORI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::OR(dst, lhs, rhs));
      }
    } else if(op == "xor") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::XORI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::XORI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::XOR(dst, lhs, rhs));
      }
    } else if(op == "shl") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::SLLI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::SLLI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::SLL(dst, lhs, rhs));
      }
    } else if(op == "ashr") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::SRAI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::SRAI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::SRA(dst, lhs, rhs));
      }
    } else if(op == "lshr") {
      if(bin.lhs.is_imm() && !bin.rhs.is_imm() && is_i12(bin.lhs.as_imm())) {
        bb.instructions.back() = RV64I::SRLI(dst, rhs, bin.lhs.as_imm());
      } else if(bin.rhs.is_imm() && !bin.lhs.is_imm() && is_i12(bin.rhs.as_imm())) {
        bb.instructions.back() = RV64I::SRLI(dst, lhs, bin.rhs.as_imm());
      } else {
        bb.instructions.push_back(RV64I::SRL(dst, lhs, rhs));
      }
    } else if(op == "icmp eq") {
      bb.instructions.push_back(RV64I::XOR(kTmpRd, lhs, rhs));
      bb.instructions.push_back(RV64I::SLTIU(dst, kTmpRd, 1));
    } else if(op == "icmp ne") {
      bb.instructions.push_back(RV64I::XOR(kTmpRd, lhs, rhs));
      bb.instructions.push_back(RV64I::SLTU(dst, PhysReg::zero, kTmpRd));
    } else if(op == "icmp slt") {
      bb.instructions.push_back(RV64I::SLT(dst, lhs, rhs));
    } else if(op == "icmp sgt") {
      bb.instructions.push_back(RV64I::SLT(dst, rhs, lhs));
    } else if(op == "icmp sle") {
      bb.instructions.push_back(RV64I::SLT(kTmpRd, rhs, lhs));
      bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
    } else if(op == "icmp sge") {
      bb.instructions.push_back(RV64I::SLT(kTmpRd, lhs, rhs));
      bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
    } else if(op == "icmp ult") {
      bb.instructions.push_back(RV64I::SLTU(dst, lhs, rhs));
    } else if(op == "icmp ugt") {
      bb.instructions.push_back(RV64I::SLTU(dst, rhs, lhs));
    } else if(op == "icmp ule") {
      bb.instructions.push_back(RV64I::SLTU(kTmpRd, rhs, lhs));
      bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
    } else if(op == "icmp uge") {
      bb.instructions.push_back(RV64I::SLTU(kTmpRd, lhs, rhs));
      bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
    } else {
      throw std::runtime_error("Unrecognized binary operator" + op);
    }

    emit_spill_store_if_needed(bin.dst, alloc, bb, kTmpRs2);
  }

  void generate_load(const ir::LoadInst& load, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
    PhysReg ptr = oper_phys_src_rs1(load.ptr.as_reg(), alloc, bb);
    PhysReg dst = oper_phys_dst(load.dst, alloc);

    switch(load.load_type.size()) {
    case 1: bb.instructions.push_back(RV64I::LB(dst, ptr, 0));
      break;
    case 2: bb.instructions.push_back(RV64I::LH(dst, ptr, 0));
      break;
    case 4: bb.instructions.push_back(RV64I::LW(dst, ptr, 0));
      break;
    case 8: bb.instructions.push_back(RV64I::LD(dst, ptr, 0));
      break;
    default: throw std::runtime_error("Impossible value size");
    }

    emit_spill_store_if_needed(load.dst, alloc, bb, kTmpRs2);
  }

  void generate_store(const ir::StoreInst& store, const AllocationResult& alloc,
                      AsmBasicBlock& bb) {
    PhysReg val = store.value.is_imm() ? PhysReg::zero : oper_phys_src_rs1(store.value.as_reg(), alloc, bb);
    PhysReg ptr = oper_phys_src_rs2(store.ptr.as_reg(), alloc, bb);

    if(store.value.is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs1, store.value.as_imm()));
      val = kTmpRs1;
    }
    switch(store.value.type.size()) {
    case 1: bb.instructions.push_back(RV64I::SB(val, ptr, 0));
      break;
    case 2: bb.instructions.push_back(RV64I::SH(val, ptr, 0));
      break;
    case 4: bb.instructions.push_back(RV64I::SW(val, ptr, 0));
      break;
    case 8: bb.instructions.push_back(RV64I::SD(val, ptr, 0));
      break;
    default: throw std::runtime_error("Impossible value size");
    }
  }

  void generate_call(const ir::CallInst& call, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
    bb.instructions.push_back(RV64I::LineComment("Start call preparation"));

    std::optional<PhysReg> caller_save_result;

    // caller-save spill
    auto it = alloc.caller_to_save.find(&call);
    if(it != alloc.caller_to_save.end()) {
      if(call.dst.has_value()
        && alloc.mapping.at(*call.dst).is_reg()
        && it->second.contains(alloc.mapping.at(*call.dst).as_reg()))
        caller_save_result = alloc.mapping.at(*call.dst).as_reg();
      int offset = alloc.caller_save_offset();
      for(auto pr: it->second) {
        // This optimization is not reflected in AllocationResult calc. Tired to optimize...
        if(caller_save_result.has_value() && *caller_save_result == pr) continue;
        try_sd(pr, PhysReg::sp, offset, bb, kTmpRd);
        // bb.instructions.push_back(RV64I::SD(pr, PhysReg::sp, offset));
        offset += 8;
      }
    }

    // set args (8+)
    for(size_t i = 8; i < call.args.size(); ++i) {
      int offset = (i - call.args.size()) * 8; // absolutely not need try_sd
      if(call.args[i].is_imm()) {
        bb.instructions.push_back(RV64I::LI(kTmpRs1, call.args[i].as_imm()));
        try_sd(kTmpRs1, PhysReg::sp, offset, bb, kTmpRd);
        // bb.instructions.push_back(RV64I::SD(kTmpRs1, PhysReg::sp, offset));
      } else {
        PhysReg src = oper_phys_src_rs1(call.args[i].as_reg(), alloc, bb);
        try_sd(src, PhysReg::sp, offset, bb, kTmpRd);
        // bb.instructions.push_back(RV64I::SD(src, PhysReg::sp, offset));
      }
    }

    // set args (8-)
    std::unordered_map<Location, Location, Location::Hash> dst_to_src;
    for(size_t i = 0; i < call.args.size() && i < 8; ++i) {
      auto& arg = call.args[i];
      auto dst = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + i);
      dst_to_src.emplace(
        Location::make_reg(dst),
        arg.is_imm() ? Location::make_imm(arg.as_imm()) : alloc.mapping.at(arg.as_reg()));
    }

    parallel_assignment(std::move(dst_to_src), bb);

    // call. No mangle here
    bb.instructions.push_back(RV64I::CALL(mangle_func_name(call.func_name)));

    // return value
    if(call.dst.has_value()) {
      PhysReg dst = oper_phys_dst(*call.dst, alloc);
      if(dst != PhysReg::a0) {
        bb.instructions.push_back(RV64I::MV(dst, PhysReg::a0));
      }
      emit_spill_store_if_needed(*call.dst, alloc, bb, kTmpRs2);
    }

    // caller-save restore
    if(it != alloc.caller_to_save.end()) {
      int offset = alloc.caller_save_offset();
      for(auto pr: it->second) {
        if(caller_save_result.has_value() && *caller_save_result == pr) continue;
        try_ld(pr, PhysReg::sp, offset, bb);
        // bb.instructions.push_back(RV64I::LD(pr, PhysReg::sp, offset));
        offset += 8;
      }
    }

    bb.instructions.push_back(RV64I::LineComment("End call cleaning"));
  }

  void generate_return(const ir::ReturnInst& ret, const AllocationResult& alloc,
                       AsmBasicBlock& bb, std::string func_name) {
    if(ret.ret_val) {
      if(ret.ret_val->is_imm()) {
        bb.instructions.push_back(RV64I::LI(PhysReg::a0, ret.ret_val->as_imm()));
      } else {
        PhysReg val = oper_phys_src_rs1(ret.ret_val->as_reg(), alloc, bb);
        if(val != PhysReg::a0) bb.instructions.push_back(RV64I::MV(PhysReg::a0, val));
      }
    }
    bb.instructions.push_back(RV64I::J(mangle_label(func_name, kEpilogueLabel)));
  }

  void generate_branch(const ir::BranchInst& br, const AllocationResult& alloc,
                       AsmBasicBlock& bb, std::string func_name) {
    bb.instructions.push_back(RV64I::J(mangle_label(func_name, br.label.to_str())));
  }

  void generate_cond_br(const ir::CondBranchInst& cond, const AllocationResult& alloc,
                        AsmBasicBlock& bb, std::string func_name) {
    if(cond.cond.is_imm()) {
      bb.instructions.push_back(RV64I::J(
        mangle_label(func_name, cond.cond.as_imm() ? cond.true_label.to_str() : cond.false_label.to_str())));
      return;
    }
    PhysReg c = oper_phys_src_rs1(cond.cond.as_reg(), alloc, bb);
    bb.instructions.push_back(RV64I::BNEZ(c, mangle_label(func_name, cond.true_label.to_str())));
    bb.instructions.push_back(RV64I::J(mangle_label(func_name, cond.false_label.to_str())));
  }

  void generate_cast(const ir::CastInst& cast, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
    PhysReg src = cast.src.is_imm() ? PhysReg::zero : oper_phys_src_rs1(cast.src.as_reg(), alloc, bb);
    if(cast.src.is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs1, cast.src.as_imm()));
      src = kTmpRs1;
    }
    PhysReg dst = oper_phys_dst(cast.dst, alloc);
    if(dst != src) bb.instructions.push_back(RV64I::MV(dst, src));
    emit_spill_store_if_needed(cast.dst, alloc, bb, kTmpRs2);
  }

  void generate_gep(const ir::GEPInst& gep, const AllocationResult& alloc,
                    AsmBasicBlock& bb) {
    PhysReg dst = oper_phys_dst(gep.dst, alloc);
    auto tp = gep.base_type.type();

    if(gep.indices.size() == 1) {
      if(gep.indices[0].is_imm()) {
        throw std::runtime_error("Uhh.. Not considered for 1-index gep with imm index yet");
      }
      auto inner_size = tp->size();
      if((inner_size & (inner_size - 1)) == 0) {
        auto shift = __builtin_ctzll(inner_size);
        if(shift == 0) {
          PhysReg index = oper_phys_src_rs2(gep.indices[0].as_reg(), alloc, bb);
          PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
          bb.instructions.push_back(RV64I::ADD(dst, base, index));
        } else {
          PhysReg index = oper_phys_src_rs2(gep.indices[0].as_reg(), alloc, bb);
          bb.instructions.push_back(RV64I::SLLI(kTmpRs2, index, shift));
          PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
          bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs2));
        }
      } else {
        bb.instructions.push_back(RV64I::LI(kTmpRs1, inner_size));
        PhysReg index = oper_phys_src_rs2(gep.indices[0].as_reg(), alloc, bb);
        bb.instructions.push_back(RV64I::MUL(kTmpRs2, index, kTmpRs1));
        PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
        bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs2));
      }
    } else if(gep.indices.size() == 2) {
      if(gep.indices[0].is_reg() || gep.indices[0].as_imm() != 0) {
        throw std::runtime_error("Invalid GEP for 2 indices with the first not 0");
      }
      if(gep.indices[1].is_imm()) {
        auto offset = static_cast<std::int64_t>(tp->offset_at(static_cast<int>(gep.indices[1].as_imm())));
        PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
        try_addi(dst, base, offset, bb, kTmpRs2);
      } else {
        auto inner_size = tp.get<stype::ArrayType>()->inner()->size();
        if((inner_size & (inner_size - 1)) == 0) {
          auto shift = __builtin_ctzll(inner_size);
          if(shift == 0) {
            PhysReg index = oper_phys_src_rs2(gep.indices[1].as_reg(), alloc, bb);
            PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
            bb.instructions.push_back(RV64I::ADD(dst, base, index));
          } else {
            PhysReg index = oper_phys_src_rs2(gep.indices[1].as_reg(), alloc, bb);
            bb.instructions.push_back(RV64I::SLLI(kTmpRs2, index, shift));
            PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
            bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs2));
          }
        } else {
          bb.instructions.push_back(RV64I::LI(kTmpRs1, inner_size));
          PhysReg index = oper_phys_src_rs2(gep.indices[1].as_reg(), alloc, bb);
          bb.instructions.push_back(RV64I::MUL(kTmpRs2, index, kTmpRs1));
          PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
          bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs2));
        }
      }
    } else {
      throw std::runtime_error("Invalid GEP for not having 1 or 2 indices");
    }

    emit_spill_store_if_needed(gep.dst, alloc, bb, kTmpRs2);
  }
} // anonymous namespace

/************************************* AsmGenerator **********************************************/

AsmGenerator::AsmGenerator(const ir::IrPack& ir_pack) : _ir_pack(ir_pack) {}

AsmPack AsmGenerator::generate() {
  AsmPack pack;
  for(const auto& func: _ir_pack.function_packs) {
    pack.functions.emplace_back(generate_func(func));
  }
  return pack;
}

AsmFunction AsmGenerator::generate_func(const ir::FunctionPack& func) {
  auto alloc = allocate_registers(func);

  _asm_func = {};
  _asm_func.name = mangle_func_name(func.ident);

  generate_prologue(func, alloc);

  for(const auto& bb: func.basic_block_packs) {
    generate_block(bb, alloc);
  }

  std::unordered_map<AsmBasicBlock*, std::unordered_map<Location, Location, Location::Hash>>
    dst_to_src_collection;
  for(const auto& bb: func.basic_block_packs) {
    for(const auto& inst: bb.instructions) {
      if(auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get())) {
        for(const auto& [op, label]: phi->incoming) {
          std::string target_label = mangle_label(_asm_func.name, label.to_str());
          for(auto& asm_bb: _asm_func.blocks) {
            if(asm_bb.label != target_label) continue;
            dst_to_src_collection[&asm_bb].emplace(
              alloc.mapping.at(phi->dst),
              op.is_imm() ? Location::make_imm(op.as_imm()) : alloc.mapping.at(op.as_reg())
            );
          }
        }
      } else break;
    }
  }

  // ir block must end with jump / br
  // asm block must end with "j label" or "bnez %res label"
  // warning: this property might be deprecated later.

  for(auto& [asm_bb, dst_to_src]: dst_to_src_collection) {
    auto& instrs = asm_bb->instructions;

    if(instrs.size() >= 2 && instrs[instrs.size() - 2].opcode == "bnez") {
      throw std::runtime_error("Invalid circumstance: critical edge");
    }
    AsmInstruction bj_inst = instrs.back();
    instrs.pop_back();

    asm_bb->instructions.push_back(RV64I::LineComment("Phi connections"));
    auto copy = dst_to_src;
    parallel_assignment(std::move(dst_to_src), *asm_bb);
    instrs.push_back(bj_inst);
  }

  generate_epilogue(alloc);

  return std::move(_asm_func);
}

void AsmGenerator::generate_prologue(const ir::FunctionPack& func, const AllocationResult& alloc) {
  AsmBasicBlock prologue;
  int frame = static_cast<int>(alloc.total_frame_size);

  // Alloc data

  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("spill func args num: {},", alloc.spill_args_num),
                alloc.spill_args_offset(), alloc.spill_args_offset(),
                alloc.spill_args_offset() + alloc.spill_args_num * 8)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("local var size: {},", alloc.local_var_size),
                alloc.local_var_offset(), alloc.local_var_offset() + alloc.local_var_size)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("return addr size: {},", alloc.ra_area_needed()),
                alloc.return_addr_offset(), alloc.return_addr_offset() + alloc.ra_area_needed())));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("callee save reg num: {} / {},", alloc.callee_saved_used.size(), kCalleeSaveRegs.size()),
                alloc.callee_save_offset(), alloc.callee_save_offset() + alloc.callee_saved_used.size() * 8)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("max caller save reg num: {} / {},", alloc.max_caller_save_num, kCallerSaveRegs.size()),
                alloc.caller_save_offset(), alloc.caller_save_offset() + alloc.max_caller_save_num * 8)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("{:35} range: {:8}(sp) - {:8}(sp)",
                std::format("spill area size: {},", alloc.spill_area_size),
                alloc.spill_regs_offset(), alloc.spill_regs_offset() + alloc.spill_area_size)));

  if(frame > 0) {
    // sp -= frame_size
    try_addi(PhysReg::sp, PhysReg::sp, -frame, prologue, kTmpRd);

    // callee-saved
    int offset = alloc.callee_save_offset();
    for(auto reg: alloc.callee_saved_used) {
      try_sd(reg, PhysReg::sp, offset, prologue, kTmpRd);
      // prologue.instructions.push_back(RV64I::SD(reg, PhysReg::sp, offset));
      offset += 8;
    }

    // ra
    bool is_leaf = alloc.caller_to_save.empty();
    if(!is_leaf) {
      try_sd(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset(), prologue, kTmpRd);
      // prologue.instructions.push_back(RV64I::SD(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset()));
    }

    // move func param
    std::unordered_map<Location, Location, Location::Hash> dst_to_src;
    for(int i = 0; i < 8 && i < func.param_num(); ++i) {
      PhysReg pr = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + i);
      Location loc = alloc.mapping.at(i);
      dst_to_src.emplace(loc, Location::make_reg(pr));
    }
    parallel_assignment(std::move(dst_to_src), prologue);
  }

  _asm_func.blocks.push_back(std::move(prologue));
}

void AsmGenerator::generate_epilogue(const AllocationResult& alloc) {
  AsmBasicBlock epilogue;
  epilogue.label = mangle_label(_asm_func.name, kEpilogueLabel);
  int frame = static_cast<int>(alloc.total_frame_size);

  if(frame > 0) {
    // ra
    bool is_leaf = alloc.caller_to_save.empty();
    if(!is_leaf) {
      try_ld(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset(), epilogue);
      // epilogue.instructions.push_back(RV64I::LD(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset()));
    }

    // callee-saved
    int offset = alloc.callee_save_offset();
    for(auto reg: alloc.callee_saved_used) {
      try_ld(reg, PhysReg::sp, offset, epilogue);
      // epilogue.instructions.push_back(RV64I::LD(reg, PhysReg::sp, offset));
      offset += 8;
    }

    // sp += frame_size
    try_addi(PhysReg::sp, PhysReg::sp, frame, epilogue, kTmpRd);
  }

  epilogue.instructions.push_back(RV64I::RET());
  _asm_func.blocks.push_back(std::move(epilogue));
}

void AsmGenerator::generate_block(const ir::BasicBlockPack& bb, const AllocationResult& alloc) {
  AsmBasicBlock asm_bb;
  asm_bb.label = mangle_label(_asm_func.name, bb.label.to_str());

  for(const auto& inst: bb.instructions) {
    if(dynamic_cast<const ir::PhiInst*>(inst.get())) continue;
    generate_instruction(inst.get(), asm_bb, alloc);
  }

  _asm_func.blocks.push_back(std::move(asm_bb));
}

void AsmGenerator::generate_instruction(const ir::Instruction* inst, AsmBasicBlock& bb,
                                        const AllocationResult& alloc) {
  // Alloca do not need handling; Phi is handled in the main process
  // bb.instructions.push_back(RV64I::LineComment(std::format("Instr {}", inst->instr_no)));
  bool to_record = true;
  if(auto* bin = dynamic_cast<const ir::BinaryOpInst*>(inst)) {
    generate_binary_op(*bin, alloc, bb);
  } else if(auto* load = dynamic_cast<const ir::LoadInst*>(inst)) {
    generate_load(*load, alloc, bb);
  } else if(auto* store = dynamic_cast<const ir::StoreInst*>(inst)) {
    generate_store(*store, alloc, bb);
  } else if(auto* call = dynamic_cast<const ir::CallInst*>(inst)) {
    generate_call(*call, alloc, bb);
  } else if(auto* ret = dynamic_cast<const ir::ReturnInst*>(inst)) {
    generate_return(*ret, alloc, bb, _asm_func.name);
  } else if(auto* br = dynamic_cast<const ir::BranchInst*>(inst)) {
    generate_branch(*br, alloc, bb, _asm_func.name);
  } else if(auto* cond = dynamic_cast<const ir::CondBranchInst*>(inst)) {
    generate_cond_br(*cond, alloc, bb, _asm_func.name);
  } else if(auto* cast = dynamic_cast<const ir::CastInst*>(inst)) {
    generate_cast(*cast, alloc, bb);
  } else if(auto* gep = dynamic_cast<const ir::GEPInst*>(inst)) {
    generate_gep(*gep, alloc, bb);
  } else {
    to_record = false;
  }
  if(to_record && !bb.instructions.empty())
    bb.instructions.back().comment = std::format("ir inst {} fin", inst->instr_no);
}
} // namespace rshard::backend
