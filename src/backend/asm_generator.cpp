#include "backend/asm_generator.hpp"
#include "backend/asm_instruction.hpp"
#include <deque>
#include <algorithm>

namespace rshard::backend {

namespace {

/************************************* Helper functions **********************************************/

bool is_32bit(ir::IrType irtype) {
  auto prime = irtype.type().get_if<stype::PrimeType>();
  if (!prime) return false;
  return prime->size() <= 4;
}

AsmOperand loc_to_oper(const Location& loc) {
  if (loc.is_reg()) return AsmOperand::reg(loc.as_reg());
  if (loc.is_spill()) return AsmOperand::mem(PhysReg::sp, loc.as_spill_offset());
  return AsmOperand::imm(loc.as_imm());
}

AsmOperand ir_op_to_asm(const ir::Operand& op, const AllocationResult& alloc) {
  if (op.is_imm()) return AsmOperand::imm(op.value);
  return loc_to_oper(alloc.mapping.at(op.value));
}

PhysReg oper_phys_dst(ir::reg_id_t vreg, const AllocationResult& alloc) {
  const auto& loc = alloc.mapping.at(vreg);
  return loc.is_reg() ? loc.as_reg() : kTmpRd;
}

PhysReg oper_phys_src_rs1(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
  const auto& loc = alloc.mapping.at(vreg);
  if (loc.is_reg()) return loc.as_reg();
  bb.instructions.push_back(RV64I::LD(kTmpRs1, PhysReg::sp, loc.as_spill_offset()));
  return kTmpRs1;
}

PhysReg oper_phys_src_rs2(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
  const auto& loc = alloc.mapping.at(vreg);
  if (loc.is_reg()) return loc.as_reg();
  bb.instructions.push_back(RV64I::LD(kTmpRs2, PhysReg::sp, loc.as_spill_offset()));
  return kTmpRs2;
}

void emit_spill_store_if_needed(ir::reg_id_t vreg, const AllocationResult& alloc, AsmBasicBlock& bb) {
  const auto& loc = alloc.mapping.at(vreg);
  if (loc.is_spill()) {
    bb.instructions.push_back(RV64I::SD(kTmpRd, PhysReg::sp, loc.as_spill_offset()));
  }
}

/************************************* IrInstr -> AsmInstr **********************************************/

void generate_binary_op(const ir::BinaryOpInst& bin, const AllocationResult& alloc,
                        AsmBasicBlock& bb) {
  PhysReg lhs = bin.lhs.is_imm() ? PhysReg::zero : oper_phys_src_rs1(bin.lhs.as_reg(), alloc, bb);
  PhysReg rhs = bin.rhs.is_imm() ? PhysReg::zero : oper_phys_src_rs2(bin.rhs.as_reg(), alloc, bb);
  PhysReg dst = oper_phys_dst(bin.dst, alloc);

  if (bin.lhs.is_imm()) { bb.instructions.push_back(RV64I::LI(kTmpRs1, bin.lhs.as_imm())); lhs = kTmpRs1; }
  if (bin.rhs.is_imm()) { bb.instructions.push_back(RV64I::LI(kTmpRs2, bin.rhs.as_imm())); rhs = kTmpRs2; }

  const std::string& op = bin.op;

  if (op == "add") {
    bb.instructions.push_back(RV64I::ADD(dst, lhs, rhs));
  } else if (op == "sub") {
    bb.instructions.push_back(RV64I::SUB(dst, lhs, rhs));
  } else if (op == "mul") {
    bb.instructions.push_back(RV64I::MUL(dst, lhs, rhs));
  } else if (op == "sdiv") {
    bb.instructions.push_back(RV64I::DIV(dst, lhs, rhs));
  } else if (op == "srem") {
    bb.instructions.push_back(RV64I::REM(dst, lhs, rhs));
  } else if (op == "and") {
    bb.instructions.push_back(RV64I::AND(dst, lhs, rhs));
  } else if (op == "or") {
    bb.instructions.push_back(RV64I::OR(dst, lhs, rhs));
  } else if (op == "xor") {
    bb.instructions.push_back(RV64I::XOR(dst, lhs, rhs));
  } else if (op == "shl") {
    bb.instructions.push_back(RV64I::SLL(dst, lhs, rhs));
  } else if (op == "ashr") {
    bb.instructions.push_back(RV64I::SRA(dst, lhs, rhs));
  } else if (op == "lshr") {
    bb.instructions.push_back(RV64I::SRL(dst, lhs, rhs));
  } else if (op == "eq") {
    bb.instructions.push_back(RV64I::XOR(kTmpRd, lhs, rhs));
    bb.instructions.push_back(RV64I::SLTIU(dst, kTmpRd, 1));
  } else if (op == "ne") {
    bb.instructions.push_back(RV64I::XOR(kTmpRd, lhs, rhs));
    bb.instructions.push_back(RV64I::SLTU(dst, PhysReg::zero, kTmpRd));
  } else if (op == "slt") {
    bb.instructions.push_back(RV64I::SLT(dst, lhs, rhs));
  } else if (op == "sgt") {
    bb.instructions.push_back(RV64I::SLT(dst, rhs, lhs));
  } else if (op == "sle") {
    bb.instructions.push_back(RV64I::SLT(kTmpRd, rhs, lhs));
    bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
  } else if (op == "sge") {
    bb.instructions.push_back(RV64I::SLT(kTmpRd, lhs, rhs));
    bb.instructions.push_back(RV64I::XORI(dst, kTmpRd, 1));
  }

  emit_spill_store_if_needed(bin.dst, alloc, bb);
}

void generate_load(const ir::LoadInst& load, const AllocationResult& alloc,
                   AsmBasicBlock& bb) {
  PhysReg ptr = oper_phys_src_rs1(load.ptr.as_reg(), alloc, bb);
  PhysReg dst = oper_phys_dst(load.dst, alloc);

  if (is_32bit(load.load_type))
    bb.instructions.push_back(RV64I::LW(dst, ptr, 0));
  else
    bb.instructions.push_back(RV64I::LD(dst, ptr, 0));

  emit_spill_store_if_needed(load.dst, alloc, bb);
}

void generate_store(const ir::StoreInst& store, const AllocationResult& alloc,
                    AsmBasicBlock& bb) {
  PhysReg val = store.value.is_imm() ? PhysReg::zero : oper_phys_src_rs1(store.value.as_reg(), alloc, bb);
  PhysReg ptr = oper_phys_src_rs2(store.ptr.as_reg(), alloc, bb);

  if (store.value.is_imm()) {
    bb.instructions.push_back(RV64I::LI(kTmpRs1, store.value.as_imm()));
    val = kTmpRs1;
  }

  if (is_32bit(store.value.type))
    bb.instructions.push_back(RV64I::SW(val, ptr, 0));
  else
    bb.instructions.push_back(RV64I::SD(val, ptr, 0));
}

void generate_call(const ir::CallInst& call, const AllocationResult& alloc,
                   AsmBasicBlock& bb) {
  // 8+ 参数压栈
  for (size_t i = 8; i < call.args.size(); ++i) {
    int offset = static_cast<int>((i - 8) * 8);
    if (call.args[i].is_imm()) {
      bb.instructions.push_back(RV64I::LI(kTmpRs1, call.args[i].as_imm()));
      bb.instructions.push_back(RV64I::SD(kTmpRs1, PhysReg::sp, offset));
    } else {
      PhysReg arg_val = oper_phys_src_rs1(call.args[i].as_reg(), alloc, bb);
      bb.instructions.push_back(RV64I::SD(arg_val, PhysReg::sp, offset));
    }
  }

  // 8- 参数：收集源，拆轮换
  std::unordered_map<PhysReg, Location> source;
  for (size_t i = 0; i < call.args.size() && i < 8; ++i) {
    auto arg_reg = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + i);
    if (call.args[i].is_imm()) {
      source.emplace(arg_reg, Location::make_imm(call.args[i].as_imm()));
    } else {
      ir::reg_id_t vreg = call.args[i].as_reg();
      const auto& loc = alloc.mapping.at(vreg);
      if (loc.is_reg()) {
        if (arg_reg != loc.as_reg()) source.emplace(arg_reg, Location::make_reg(loc.as_reg()));
      } else {
        source.emplace(arg_reg, Location::make_spill(loc.as_spill_offset()));
      }
    }
  }

  while (!source.empty()) {
    PhysReg choice{};
    std::vector<std::pair<PhysReg, Location>> worklist;
    for (const auto& [dst, src] : source) {
      if (src.is_reg()) {
        choice = dst;
        worklist.emplace_back(dst, src);
        source.erase(dst);
        break;
      }
    }
    if (worklist.empty()) break;

    auto record_it = bb.instructions.insert(bb.instructions.end(), RV64I::MV(kTmpRd, choice));

    std::vector<PhysReg> loop_end;
    while (!worklist.empty()) {
      auto [dst, src] = worklist.back();
      worklist.pop_back();

      if (src.is_reg()) {
        PhysReg pr = src.as_reg();
        bb.instructions.push_back(RV64I::MV(dst, pr));
        if (auto it = source.find(pr); it != source.end()) {
          auto& loc = it->second;
          if (loc.is_reg() && loc.as_reg() == choice) loop_end.push_back(pr);
          else worklist.emplace_back(pr, loc);
          source.erase(it);
        }
      } else if (src.is_imm()) {
        bb.instructions.push_back(RV64I::LI(dst, src.as_imm()));
      } else {
        bb.instructions.push_back(RV64I::LD(kTmpRs1, PhysReg::sp, src.as_spill_offset()));
        bb.instructions.push_back(RV64I::MV(dst, kTmpRs1));
      }
    }

    if (loop_end.empty()) bb.instructions.erase(record_it);
    else for (const auto& dst : loop_end) bb.instructions.push_back(RV64I::MV(dst, kTmpRd));
  }

  for (const auto& [dst, src] : source) {
    if (src.is_spill()) {
      bb.instructions.push_back(RV64I::LD(kTmpRs1, PhysReg::sp, src.as_spill_offset()));
      bb.instructions.push_back(RV64I::MV(dst, kTmpRs1));
    } else {
      bb.instructions.push_back(RV64I::LI(dst, src.as_imm()));
    }
  }

  bb.instructions.push_back(RV64I::CALL(call.func_name));

  if (call.dst.has_value()) {
    PhysReg dst = oper_phys_dst(*call.dst, alloc);
    if (dst != PhysReg::a0) bb.instructions.push_back(RV64I::MV(dst, PhysReg::a0));
    emit_spill_store_if_needed(*call.dst, alloc, bb);
  }
}

void generate_return(const ir::ReturnInst& ret, const AllocationResult& alloc,
                     AsmBasicBlock& bb) {
  if (ret.ret_val) {
    if (ret.ret_val->is_imm()) {
      bb.instructions.push_back(RV64I::LI(PhysReg::a0, ret.ret_val->as_imm()));
    } else {
      PhysReg val = oper_phys_src_rs1(ret.ret_val->as_reg(), alloc, bb);
      if (val != PhysReg::a0) bb.instructions.push_back(RV64I::MV(PhysReg::a0, val));
    }
  }
  bb.instructions.push_back(RV64I::J(AsmGenerator::kEpilogueLabel));
}

void generate_cond_branch(const ir::CondBranchInst& cond, const AllocationResult& alloc,
                          AsmBasicBlock& bb) {
  PhysReg c = oper_phys_src_rs1(cond.cond.as_reg(), alloc, bb);
  bb.instructions.push_back(RV64I::BNEZ(c, cond.true_label.to_str()));
  bb.instructions.push_back(RV64I::J(cond.false_label.to_str()));
}

void generate_cast(const ir::CastInst& cast, const AllocationResult& alloc,
                   AsmBasicBlock& bb) {
  PhysReg src = cast.src.is_imm() ? PhysReg::zero : oper_phys_src_rs1(cast.src.as_reg(), alloc, bb);
  if (cast.src.is_imm()) { bb.instructions.push_back(RV64I::LI(kTmpRs1, cast.src.as_imm())); src = kTmpRs1; }
  PhysReg dst = oper_phys_dst(cast.dst, alloc);
  if (dst != src) bb.instructions.push_back(RV64I::MV(dst, src));
  emit_spill_store_if_needed(cast.dst, alloc, bb);
}

void generate_alloca(const ir::AllocaInst& alloca_inst, const AllocationResult& alloc_res,
                     AsmBasicBlock& bb) {
  // Alloca 映射到 sp + offset 的地址
  int32_t offset = alloc_res.mapping.at(alloca_inst.dst).as_spill_offset();
  PhysReg dst = oper_phys_dst(alloca_inst.dst, alloc_res);
  bb.instructions.push_back(RV64I::ADDI(dst, PhysReg::sp, offset));
  emit_spill_store_if_needed(alloca_inst.dst, alloc_res, bb);
}

void generate_gep(const ir::GEPInst& gep, const AllocationResult& alloc,
                  AsmBasicBlock& bb) {
  PhysReg dst = oper_phys_dst(gep.dst, alloc);
  auto tp = gep.base_type.type();

  if (gep.indices.size() == 1) {
    // 指针/标量: dst = base + idx * sizeof(T)
    auto inner_size = tp->size();
    bb.instructions.push_back(RV64I::LI(kTmpRs1, inner_size));
    PhysReg idx = oper_phys_src_rs2(gep.indices[0].as_reg(), alloc, bb);
    bb.instructions.push_back(RV64I::MUL(kTmpRs1, idx, kTmpRs1));
    PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
    bb.instructions.push_back(RV64I::ADD(dst, base, kTmpRs1));
  } else if (gep.indices.size() == 2) {
    auto idx = gep.indices[1];
    if (idx.is_imm()) {
      auto offset = static_cast<std::int64_t>(tp->offset_at(static_cast<int>(idx.as_imm())));
      PhysReg base = oper_phys_src_rs1(gep.ptr.as_reg(), alloc, bb);
      bb.instructions.push_back(RV64I::ADDI(dst, base, offset));
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

  for (const auto& [op, label] : phi.incoming) {
    if (op.is_imm()) {
      std::string target_label = ".L_" + asm_func.name + "_" + label.to_str();
      for (auto& asm_bb : asm_func.blocks) {
        if (asm_bb.label != target_label) continue;
        auto pos = asm_bb.instructions.end();
        if (!asm_bb.instructions.empty()) --pos;
        if (dst_loc.is_reg()) {
          asm_bb.instructions.insert(pos, RV64I::LI(dst_loc.as_reg(), op.as_imm()));
        } else {
          asm_bb.instructions.insert(pos, RV64I::LI(kTmpRd, op.as_imm()));
          asm_bb.instructions.insert(pos + 1, RV64I::SD(kTmpRd, PhysReg::sp, dst_loc.as_spill_offset()));
        }
        break;
      }
      continue;
    }

    const auto& src_loc = alloc.mapping.at(op.as_reg());
    if (dst_loc == src_loc) continue;

    std::string target_label = ".L_" + asm_func.name + "_" + label.to_str();
    for (auto& asm_bb : asm_func.blocks) {
      if (asm_bb.label != target_label) continue;

      auto pos = asm_bb.instructions.end();
      if (!asm_bb.instructions.empty()) --pos;

      if (dst_loc.is_reg() && src_loc.is_reg()) {
        asm_bb.instructions.insert(pos, RV64I::MV(dst_loc.as_reg(), src_loc.as_reg()));
      } else if (dst_loc.is_reg() && src_loc.is_spill()) {
        asm_bb.instructions.insert(pos, RV64I::LD(dst_loc.as_reg(), PhysReg::sp, src_loc.as_spill_offset()));
      } else if (dst_loc.is_spill() && src_loc.is_reg()) {
        asm_bb.instructions.insert(pos, RV64I::SD(src_loc.as_reg(), PhysReg::sp, dst_loc.as_spill_offset()));
      } else if (dst_loc.is_spill() && src_loc.is_spill()) {
        asm_bb.instructions.insert(pos, RV64I::LD(kTmpRd, PhysReg::sp, src_loc.as_spill_offset()));
        asm_bb.instructions.insert(pos + 1, RV64I::SD(kTmpRd, PhysReg::sp, dst_loc.as_spill_offset()));
      }
      break;
    }
  }
}

} // anonymous namespace

/************************************* AsmGenerator **********************************************/

AsmGenerator::AsmGenerator(const ir::IrPack& ir_pack) : _ir_pack(ir_pack) {}

AsmPack AsmGenerator::generate() {
  AsmPack pack;
  for (const auto& func : _ir_pack.function_packs) {
    pack.functions.emplace_back(generate_func(func));
  }
  return pack;
}

AsmFunction AsmGenerator::generate_func(const ir::FunctionPack& func) {
  auto alloc = allocate_registers(func);

  _asm_func = {};
  _asm_func.name = func.ident;
  _asm_func.stack_frame_size = alloc.total_frame_size;
  _asm_func.callee_saved_regs = std::vector<PhysReg>(
    alloc.callee_saved_used.begin(), alloc.callee_saved_used.end());
  std::sort(_asm_func.callee_saved_regs.begin(), _asm_func.callee_saved_regs.end(),
            [](PhysReg a, PhysReg b) { return static_cast<uint8_t>(a) < static_cast<uint8_t>(b); });
  _asm_func.allocation_result = alloc;

  generate_prologue(alloc);

  for (const auto& bb : func.basic_block_packs) {
    generate_block(bb, alloc);
  }

  for (const auto& bb : func.basic_block_packs) {
    for (const auto& inst : bb.instructions) {
      if (auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get())) {
        handle_phi(*phi, _asm_func, alloc);
      } else break;
    }
  }

  AsmBasicBlock epilogue;
  generate_epilogue(alloc, epilogue);
  _asm_func.blocks.push_back(std::move(epilogue));

  return std::move(_asm_func);
}

void AsmGenerator::generate_prologue(const AllocationResult& alloc) {
  AsmBasicBlock prologue;
  prologue.label = "";
  int frame = static_cast<int>(alloc.total_frame_size);

  if (frame > 0) {
    prologue.instructions.push_back(RV64I::ADDI(PhysReg::sp, PhysReg::sp, -frame));
    prologue.instructions.push_back(RV64I::SD(PhysReg::ra, PhysReg::sp, frame - 8));

    int offset = frame - 16;
    for (auto reg : _asm_func.callee_saved_regs) {
      prologue.instructions.push_back(RV64I::SD(reg, PhysReg::sp, offset));
      offset -= 8;
    }
  }

  _asm_func.blocks.push_back(std::move(prologue));
}

void AsmGenerator::generate_epilogue(const AllocationResult& alloc, AsmBasicBlock& bb) {
  bb.label = ".L_" + _asm_func.name + "_epilogue";
  int frame = static_cast<int>(alloc.total_frame_size);

  if (frame > 0) {
    int offset = frame - 16;
    for (auto reg : _asm_func.callee_saved_regs) {
      bb.instructions.push_back(RV64I::LD(reg, PhysReg::sp, offset));
      offset -= 8;
    }
    bb.instructions.push_back(RV64I::LD(PhysReg::ra, PhysReg::sp, frame - 8));
    bb.instructions.push_back(RV64I::ADDI(PhysReg::sp, PhysReg::sp, frame));
  }

  bb.instructions.push_back(RV64I::RET());
}

void AsmGenerator::generate_block(const ir::BasicBlockPack& bb, const AllocationResult& alloc) {
  AsmBasicBlock asm_bb;
  asm_bb.label = ".L_" + _asm_func.name + "_" + bb.label.to_str();

  for (const auto& inst : bb.instructions) {
    if (dynamic_cast<const ir::PhiInst*>(inst.get())) continue;
    generate_instruction(inst.get(), asm_bb, alloc);
  }

  _asm_func.blocks.push_back(std::move(asm_bb));
}

void AsmGenerator::generate_instruction(const ir::Instruction* inst, AsmBasicBlock& bb,
                                         const AllocationResult& alloc) {
  if (auto* bin = dynamic_cast<const ir::BinaryOpInst*>(inst)) {
    generate_binary_op(*bin, alloc, bb);
  } else if (auto* load = dynamic_cast<const ir::LoadInst*>(inst)) {
    generate_load(*load, alloc, bb);
  } else if (auto* store = dynamic_cast<const ir::StoreInst*>(inst)) {
    generate_store(*store, alloc, bb);
  } else if (auto* call = dynamic_cast<const ir::CallInst*>(inst)) {
    generate_call(*call, alloc, bb);
  } else if (auto* ret = dynamic_cast<const ir::ReturnInst*>(inst)) {
    generate_return(*ret, alloc, bb);
  } else if (auto* br = dynamic_cast<const ir::BranchInst*>(inst)) {
    bb.instructions.push_back(RV64I::J(kEpilogueLabel));
  } else if (auto* cond = dynamic_cast<const ir::CondBranchInst*>(inst)) {
    generate_cond_branch(*cond, alloc, bb);
  } else if (auto* cast = dynamic_cast<const ir::CastInst*>(inst)) {
    generate_cast(*cast, alloc, bb);
  } else if (auto* gep = dynamic_cast<const ir::GEPInst*>(inst)) {
    generate_gep(*gep, alloc, bb);
  } else if (auto* alloca_inst = dynamic_cast<const ir::AllocaInst*>(inst)) {
    generate_alloca(*alloca_inst, alloc, bb);
  }
}

} // namespace rshard::backend