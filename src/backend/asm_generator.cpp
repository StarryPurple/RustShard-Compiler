#include "backend/asm_generator.hpp"
#include "backend/asm_instruction.hpp"
#include <deque>
#include <algorithm>
#include <format>

namespace rshard::backend {
namespace {
  /************************************* Helper functions **********************************************/
  constexpr std::string kEpilogueLabel = "Epilogue";

  std::string mangle_label(const std::string& func_name, const std::string& label) {
    return func_name + "." + label;
  }

  bool is_32bit(ir::IrType irtype) {
    auto prime = irtype.type().get_if<stype::PrimeType>();
    if(!prime) return false;
    return prime->size() <= 4;
  }

  AsmOperand loc_to_oper(const Location& loc) {
    if(loc.is_reg()) return AsmOperand::reg(loc.as_reg());
    if(loc.is_spill()) return AsmOperand::mem(PhysReg::sp, loc.as_spill_offset());
    throw std::runtime_error("Unreachable loc_to_oper end");
  }

  AsmOperand ir_op_to_asm(const ir::Operand& op, const AllocationResult& alloc) {
    if(op.is_imm()) return AsmOperand::imm(op.value);
    return loc_to_oper(alloc.mapping.at(op.value));
  }

  void try_sd(PhysReg reg, PhysReg base, int32_t offset, AsmBasicBlock& bb, PhysReg tmp = PhysReg::zero) {
    if(-2048 <= offset && offset <= 2047) {
      bb.instructions.push_back(RV64I::SD(reg, base, offset));
    } else {
      if(tmp == PhysReg::zero) throw std::runtime_error("try_sd not assigned with helper");
      bb.instructions.push_back(RV64I::LI(tmp, offset));
      bb.instructions.push_back(RV64I::ADD(tmp, base, tmp));
      bb.instructions.push_back(RV64I::SD(reg, tmp, 0));
    }
  }

  void try_ld(PhysReg reg, PhysReg base, int32_t offset, AsmBasicBlock& bb) {
    if(-2048 <= offset && offset <= 2047) {
      bb.instructions.push_back(RV64I::LD(reg, base, offset));
    } else {
      bb.instructions.push_back(RV64I::LI(reg, offset));
      bb.instructions.push_back(RV64I::ADD(reg, base, reg));
      bb.instructions.push_back(RV64I::LD(reg, reg, 0));
    }
  }

  void try_lea(PhysReg reg, int32_t offset, AsmBasicBlock& bb) {
    if(-2048 <= offset && offset <= 2047) {
      bb.instructions.push_back(RV64I::ADDI(reg, PhysReg::sp, offset));
    } else {
      bb.instructions.push_back(RV64I::LI(reg, offset));
      bb.instructions.push_back(RV64I::ADD(reg, PhysReg::sp, reg));
    }
  }

  void try_addi(PhysReg rd, PhysReg rs1, int32_t imm, AsmBasicBlock& bb, PhysReg tmp = PhysReg::zero) {
    if(imm >= -2048 && imm <= 2047) {
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
    // bb.instructions.push_back(RV64I::LD(kTmpRs1, PhysReg::sp, loc.as_spill_offset()));
    try_ld(kTmpRs1, PhysReg::sp, loc.as_spill_offset(), bb);
    return kTmpRs1;
  }

  PhysReg oper_phys_src_rs2(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
    const auto& loc = alloc.mapping.at(vreg);
    if(loc.is_reg()) return loc.as_reg();
    if(loc.is_addr()) {
      try_lea(kTmpRs2, loc.as_addr(), bb);
      return kTmpRs2;
    }
    // bb.instructions.push_back(RV64I::LD(kTmpRs2, PhysReg::sp, loc.as_spill_offset()));
    try_ld(kTmpRs2, PhysReg::sp, loc.as_spill_offset(), bb);
    return kTmpRs2;
  }

  // You can pass no tmp in.
  void emit_spill_store_if_needed(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb,
                                  PhysReg tmp = PhysReg::zero) {
    const auto& loc = alloc.mapping.at(vreg);
    if(loc.is_spill()) {
      try_sd(kTmpRs1, PhysReg::sp, loc.as_spill_offset(), bb, tmp);
      bb.instructions.push_back(RV64I::SD(kTmpRd, PhysReg::sp, loc.as_spill_offset()));
    }
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
      bb.instructions.push_back(RV64I::ADD(dst, lhs, rhs));
    } else if(op == "sub") {
      bb.instructions.push_back(RV64I::SUB(dst, lhs, rhs));
    } else if(op == "mul") {
      bb.instructions.push_back(RV64I::MUL(dst, lhs, rhs));
    } else if(op == "sdiv") {
      bb.instructions.push_back(RV64I::DIV(dst, lhs, rhs));
    } else if(op == "udiv") {
      bb.instructions.push_back(RV64I::UDIV(dst, lhs, rhs));
    } else if(op == "srem") {
      bb.instructions.push_back(RV64I::REM(dst, lhs, rhs));
    } else if(op == "urem") {
      bb.instructions.push_back(RV64I::UREM(dst, lhs, rhs));
    } else if(op == "and") {
      bb.instructions.push_back(RV64I::AND(dst, lhs, rhs));
    } else if(op == "or") {
      bb.instructions.push_back(RV64I::OR(dst, lhs, rhs));
    } else if(op == "xor") {
      bb.instructions.push_back(RV64I::XOR(dst, lhs, rhs));
    } else if(op == "shl") {
      bb.instructions.push_back(RV64I::SLL(dst, lhs, rhs));
    } else if(op == "ashr") {
      bb.instructions.push_back(RV64I::SRA(dst, lhs, rhs));
    } else if(op == "lshr") {
      bb.instructions.push_back(RV64I::SRL(dst, lhs, rhs));
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

    emit_spill_store_if_needed(bin.dst, alloc, bb);
  }

  void generate_load(const ir::LoadInst& load, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
    PhysReg ptr = oper_phys_src_rs1(load.ptr.as_reg(), alloc, bb);
    PhysReg dst = oper_phys_dst(load.dst, alloc);

    if(is_32bit(load.load_type))
      bb.instructions.push_back(RV64I::LW(dst, ptr, 0));
    else
      bb.instructions.push_back(RV64I::LD(dst, ptr, 0));

    emit_spill_store_if_needed(load.dst, alloc, bb);
  }

  void generate_store(const ir::StoreInst& store, const AllocationResult& alloc,
                      AsmBasicBlock& bb) {
    PhysReg val = store.value.is_imm() ? PhysReg::zero : oper_phys_src_rs1(store.value.as_reg(), alloc, bb);
    PhysReg ptr = oper_phys_src_rs2(store.ptr.as_reg(), alloc, bb);

    if(store.value.is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs1, store.value.as_imm()));
      val = kTmpRs1;
    }

    if(is_32bit(store.value.type))
      bb.instructions.push_back(RV64I::SW(val, ptr, 0));
    else
      bb.instructions.push_back(RV64I::SD(val, ptr, 0));
  }

  void generate_call(const ir::CallInst& call, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
    bb.instructions.push_back(RV64I::LineComment("Start call preparation"));
    // caller-save spill
    auto it = alloc.caller_to_save.find(&call);
    if(it != alloc.caller_to_save.end()) {
      int offset = alloc.caller_save_offset();
      for(auto pr: it->second) {
        bb.instructions.push_back(RV64I::SD(pr, PhysReg::sp, offset));
        offset += 8;
      }
    }

    // set args (8+)
    for(size_t i = 8; i < call.args.size(); ++i) {
      int offset = static_cast<int>(alloc.func_args_offset() + (i - 8) * 8);
      if(call.args[i].is_imm()) {
        bb.instructions.push_back(RV64I::LI(kTmpRs1, call.args[i].as_imm()));
        bb.instructions.push_back(RV64I::SD(kTmpRs1, PhysReg::sp, offset));
      } else {
        PhysReg src = oper_phys_src_rs1(call.args[i].as_reg(), alloc, bb);
        bb.instructions.push_back(RV64I::SD(src, PhysReg::sp, offset));
      }
    }

    // set args (8-)
    for(size_t i = 0; i < call.args.size() && i < 8; ++i) {
      auto dst = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + i);
      if(call.args[i].is_imm()) {
        bb.instructions.push_back(RV64I::LI(dst, call.args[i].as_imm()));
      } else {
        PhysReg src = oper_phys_src_rs1(call.args[i].as_reg(), alloc, bb);
        if(dst != src) {
          bb.instructions.push_back(RV64I::MV(dst, src));
        }
      }
    }

    // call. No mangle here
    bb.instructions.push_back(RV64I::CALL(call.func_name));

    // caller-save restore
    if(it != alloc.caller_to_save.end()) {
      int offset = alloc.caller_save_offset();
      for(auto pr: it->second) {
        bb.instructions.push_back(RV64I::LD(pr, PhysReg::sp, offset));
        offset += 8;
      }
    }

    // return value
    if(call.dst.has_value()) {
      PhysReg dst = oper_phys_dst(*call.dst, alloc);
      if(dst != PhysReg::a0) {
        bb.instructions.push_back(RV64I::MV(dst, PhysReg::a0));
      }
      emit_spill_store_if_needed(*call.dst, alloc, bb);
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
    emit_spill_store_if_needed(cast.dst, alloc, bb);
  }

  void generate_gep(const ir::GEPInst& gep, const AllocationResult& alloc,
                    AsmBasicBlock& bb) {
    PhysReg dst = oper_phys_dst(gep.dst, alloc);
    auto tp = gep.base_type.type();

    if(gep.indices.size() == 1) {
      auto inner_size = tp->size();
      bb.instructions.push_back(RV64I::LI(kTmpRs1, inner_size));
      PhysReg idx = oper_phys_src_rs2(gep.indices[0].as_reg(), alloc, bb);
      bb.instructions.push_back(RV64I::MUL(kTmpRs1, idx, kTmpRs1));
      PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
      bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs1));
    } else if(gep.indices.size() == 2) {
      auto idx = gep.indices[1];
      if(idx.is_imm()) {
        auto offset = static_cast<std::int64_t>(tp->offset_at(static_cast<int>(idx.as_imm())));
        PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
        try_addi(dst, base, offset, bb, kTmpRs2);
      } else {
        auto inner_size = tp.get<stype::ArrayType>()->inner()->size();
        bb.instructions.push_back(RV64I::LI(kTmpRs1, inner_size));
        PhysReg offset = oper_phys_src_rs2(idx.as_reg(), alloc, bb);
        bb.instructions.push_back(RV64I::MUL(offset, offset, kTmpRs1));
        PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
        bb.instructions.push_back(RV64I::ADD(dst, base, offset));
      }
    }

    emit_spill_store_if_needed(gep.dst, alloc, bb);
  }

  void handle_phi(const ir::PhiInst& phi, AsmFunction& asm_func,
                  const AllocationResult& alloc) {
    const auto& dst_loc = alloc.mapping.at(phi.dst);

    for(const auto& [op, label]: phi.incoming) {
      if(op.is_imm()) {
        std::string target_label = mangle_label(asm_func.name, label.to_str());
        for(auto& asm_bb: asm_func.blocks) {
          if(asm_bb.label != target_label) continue;
          auto& instrs = asm_bb.instructions;
          if(instrs.empty()) {
            throw std::runtime_error("Empty basic block");
          }
          if(dst_loc.is_reg()) {
            instrs.insert(--instrs.end(), RV64I::LI(dst_loc.as_reg(), op.as_imm()));
          } else {
            instrs.insert(--instrs.end(), RV64I::LI(kTmpRd, op.as_imm()));
            instrs.insert(--instrs.end(), RV64I::SD(kTmpRd, PhysReg::sp, dst_loc.as_spill_offset()));
          }
          break;
        }
      } else {
        const auto& src_loc = alloc.mapping.at(op.as_reg());
        if(dst_loc == src_loc) continue;

        std::string target_label = mangle_label(asm_func.name, label.to_str());
        for(auto& asm_bb: asm_func.blocks) {
          if(asm_bb.label != target_label) continue;

          auto& instrs = asm_bb.instructions;
          if(instrs.empty()) {
            throw std::runtime_error("Empty basic block");
          }

          if(dst_loc.is_reg() && src_loc.is_reg()) {
            asm_bb.instructions.insert(--instrs.end(), RV64I::MV(dst_loc.as_reg(), src_loc.as_reg()));
          } else if(dst_loc.is_reg() && src_loc.is_spill()) {
            asm_bb.instructions.insert(--instrs.end(),
                                       RV64I::LD(dst_loc.as_reg(), PhysReg::sp, src_loc.as_spill_offset()));
          } else if(dst_loc.is_spill() && src_loc.is_reg()) {
            asm_bb.instructions.insert(--instrs.end(),
                                       RV64I::SD(src_loc.as_reg(), PhysReg::sp, dst_loc.as_spill_offset()));
          } else if(dst_loc.is_spill() && src_loc.is_spill()) {
            asm_bb.instructions.insert(--instrs.end(), RV64I::LD(kTmpRd, PhysReg::sp, src_loc.as_spill_offset()));
            asm_bb.instructions.insert(--instrs.end(), RV64I::SD(kTmpRd, PhysReg::sp, dst_loc.as_spill_offset()));
          }
          break;
        }
      }
    }
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
  _asm_func.name = func.ident;

  generate_prologue(alloc);

  for(const auto& bb: func.basic_block_packs) {
    generate_block(bb, alloc);
  }

  for(const auto& bb: func.basic_block_packs) {
    for(const auto& inst: bb.instructions) {
      if(auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get())) {
        handle_phi(*phi, _asm_func, alloc);
      } else break;
    }
  }

  generate_epilogue(alloc);

  return std::move(_asm_func);
}

void AsmGenerator::generate_prologue(const AllocationResult& alloc) {
  AsmBasicBlock prologue;
  int frame = static_cast<int>(alloc.total_frame_size);

  // Alloc data

  prologue.instructions.push_back(RV64I::LineComment(
    std::format("spill area size: {}", alloc.spill_area_size)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("local var size: {}", alloc.local_var_size)));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("max caller save reg num: {} / {}", alloc.max_caller_save_num, kCallerSaveRegs.size())));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("callee save reg num: {} / {}", alloc.callee_saved_used.size(), kCalleeSaveRegs.size())));
  prologue.instructions.push_back(RV64I::LineComment(
    std::format("extra func args num: {}", alloc.spill_args_num)));

  if(frame > 0) {
    // sp -= frame_size
    try_addi(PhysReg::sp, PhysReg::sp, -frame, prologue, kTmpRd);

    // callee-saved
    int offset = alloc.callee_save_offset();
    for(auto reg: alloc.callee_saved_used) {
      prologue.instructions.push_back(RV64I::SD(reg, PhysReg::sp, offset));
      offset += 8;
    }

    // ra
    bool is_leaf = alloc.caller_to_save.empty();
    if(!is_leaf) {
      prologue.instructions.push_back(RV64I::SD(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset()));
    }
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
      epilogue.instructions.push_back(RV64I::LD(PhysReg::ra, PhysReg::sp, alloc.return_addr_offset()));
    }

    // callee-saved
    int offset = alloc.callee_save_offset();
    for(auto reg: alloc.callee_saved_used) {
      epilogue.instructions.push_back(RV64I::LD(reg, PhysReg::sp, offset));
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
  // Alloca do not need handling; Phi is handled in handle_phi
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
  }
}
} // namespace rshard::backend
