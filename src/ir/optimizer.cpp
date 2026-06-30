#include "ir/optimizer.hpp"
#include <stack>
#include <algorithm>

namespace rshard::ir {
void eliminate_single_phi(FunctionPack& func) {
  std::vector<std::pair<reg_id_t, Operand>> use_map;
  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      auto* phi = dynamic_cast<PhiInst*>(inst.get());
      if(!phi) break;
      if(phi->incoming.size() >= 2) continue;
      if(phi->incoming.size() == 1) {
        use_map.emplace_back(phi->dst, phi->incoming.front().oper);
      }
      inst.reset(); // not needed any more
    }
  }
  for(auto& bb: func.basic_block_packs) {
    auto pred = [](const std::unique_ptr<Instruction>& inst) {
      return !inst;
    };
    std::erase_if(bb.instructions, pred);
  }
  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      for(auto& [reg, oper]: use_map)
        inst->replace_use(reg, oper);
    }
  }
}

bool eliminate_deadcode(FunctionPack& func) {
  std::unordered_map<reg_id_t, Instruction*> def_map;

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      if(auto dst = inst->get_dst()) {
        def_map[*dst] = inst.get();
      }
    }
  }

  std::unordered_set<Instruction*> useful;
  std::vector<Instruction*> worklist;

  auto mark_useful = [&](Instruction* inst) {
    if(!inst) return;
    if(useful.insert(inst).second) {
      worklist.push_back(inst);
    }
  };

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      auto* ptr = inst.get();

      // initially marked "useful"
      if(dynamic_cast<StoreInst*>(ptr) ||
        dynamic_cast<CallInst*>(ptr) ||
        dynamic_cast<ReturnInst*>(ptr) ||
        dynamic_cast<BranchInst*>(ptr) ||
        dynamic_cast<CondBranchInst*>(ptr) ||
        dynamic_cast<UnreachableInst*>(ptr)) {
        mark_useful(ptr);
      }
    }
  }

  while(!worklist.empty()) {
    Instruction* inst = worklist.back();
    worklist.pop_back();

    for(reg_id_t use: inst->get_uses()) {
      auto it = def_map.find(use);
      if(it != def_map.end()) {
        mark_useful(it->second);
      }
    }
  }

  std::size_t remove_count = 0;
  for(auto& bb: func.basic_block_packs) {
    auto pred = [&](const std::unique_ptr<Instruction>& inst) {
      auto dst = inst->get_dst();
      if(!dst) return false; // must be useful
      return !useful.contains(inst.get());
    };
    remove_count += bb.instructions.size();
    std::erase_if(bb.instructions, pred);
    remove_count -= bb.instructions.size();
  }
  return remove_count > 0;
}

bool constant_fold(FunctionPack& func) {
  bool changed = false;

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      auto* binop = dynamic_cast<BinaryOpInst*>(inst.get());
      if(!binop) continue;

      Operand lhs = binop->lhs;
      Operand rhs = binop->rhs;
      StringT op = binop->op;

      std::optional<Operand> result;

      // 1: two imm: fold.
      if(lhs.is_imm() && rhs.is_imm()) {
        int64_t a = lhs.as_imm(), b = rhs.as_imm();
        if(op == "add") result = Operand::make_imm(a + b, binop->type);
        else if(op == "sub") result = Operand::make_imm(a - b, binop->type);
        else if(op == "mul") result = Operand::make_imm(a * b, binop->type);
        else if(op == "sdiv" && b != 0)
          result = Operand::make_imm(a / b, binop->type);
        else if(op == "udiv" && b != 0)
          result = Operand::make_imm(static_cast<int64_t>(static_cast<uint64_t>(a) / static_cast<uint64_t>(b)),
                                     binop->type);
        else if(op == "srem" && b != 0)
          result = Operand::make_imm(a % b, binop->type);
        else if(op == "urem" && b != 0)
          result = Operand::make_imm(static_cast<int64_t>(static_cast<uint64_t>(a) % static_cast<uint64_t>(b)),
                                     binop->type);
        else if(op == "and") result = Operand::make_imm(a & b, binop->type);
        else if(op == "or") result = Operand::make_imm(a | b, binop->type);
        else if(op == "xor") result = Operand::make_imm(a ^ b, binop->type);
        else if(op == "shl") result = Operand::make_imm(a << b, binop->type);
        else if(op == "ashr") result = Operand::make_imm(a >> b, binop->type);
        else if(op == "lshr")
          result = Operand::make_imm(static_cast<int64_t>(static_cast<uint64_t>(a) >> b), binop->type);
        else if(op == "eq") result = Operand::make_imm(a == b ? 1 : 0, binop->type);
        else if(op == "ne") result = Operand::make_imm(a != b ? 1 : 0, binop->type);
        else if(op == "sgt") result = Operand::make_imm(a > b ? 1 : 0, binop->type);
        else if(op == "sge") result = Operand::make_imm(a >= b ? 1 : 0, binop->type);
        else if(op == "slt") result = Operand::make_imm(a < b ? 1 : 0, binop->type);
        else if(op == "sle") result = Operand::make_imm(a <= b ? 1 : 0, binop->type);
      }
      // 2: arithmetically simplifiable
      else if(op == "add" || op == "sub" || op == "or" || op == "xor") {
        // X + 0 = X, X - 0 = X, X | 0 = X, X ^ 0 = X
        if(rhs.is_imm() && rhs.as_imm() == 0) {
          result = lhs;
        }
        // 0 + X = X
        if(op == "add" && lhs.is_imm() && lhs.as_imm() == 0) {
          result = rhs;
        }
        // X - X = 0
        if(op == "sub" && lhs.is_reg() && rhs.is_reg()
          && lhs.as_reg() == rhs.as_reg()) {
          result = Operand::make_imm(0, binop->type);
        }
      } else if(op == "mul") {
        // X * 1 = X
        if(rhs.is_imm() && rhs.as_imm() == 1) result = lhs;
        else if(lhs.is_imm() && lhs.as_imm() == 1) result = rhs;
          // X * 0 = 0
        else if((rhs.is_imm() && rhs.as_imm() == 0) || (lhs.is_imm() && lhs.as_imm() == 0)) {
          result = Operand::make_imm(0, binop->type);
        }
      } else if(op == "sdiv" || op == "udiv") {
        // X / 1 = X
        if(rhs.is_imm() && rhs.as_imm() == 1) result = lhs;
      } else if(op == "and") {
        // X & 0 = 0
        if((rhs.is_imm() && rhs.as_imm() == 0) || (lhs.is_imm() && lhs.as_imm() == 0)) {
          result = Operand::make_imm(0, binop->type);
        }
        // X & -1 = X
        else if(rhs.is_imm() && rhs.as_imm() == -1) result = lhs;
        else if(lhs.is_imm() && lhs.as_imm() == -1) result = rhs;
      }

      if(result) {
        reg_id_t old_dst = binop->dst;
        for(auto& bb2: func.basic_block_packs) {
          for(auto& inst2: bb2.instructions) {
            if(!inst2) continue;
            inst2->replace_use(old_dst, *result);
          }
        }
        inst.reset(); // set nullptr
        changed = true;
      }
    }
  }

  if(changed) {
    for(auto& bb: func.basic_block_packs) {
      auto pred = [](const std::unique_ptr<Instruction>& inst) {
        return !inst;
      };
      std::erase_if(bb.instructions, pred);
    }
  }

  return changed;
}

void eliminate_critical_edge(FunctionPack& func) {
  func.construct_cfg();
  auto &cfg = func.cfg;

  std::vector<std::pair<block_id_t, block_id_t>> edges_to_split;
  for(block_id_t from = 0; from < cfg.succ.size(); ++from) {
    if(cfg.succ[from].size() <= 1) continue;
    for(block_id_t to: cfg.succ[from]) {
      if(cfg.pred[to].size() > 1) {
        edges_to_split.push_back({from, to});
      }
    }
  }
  if(edges_to_split.empty()) return;

  cfg.valid = false;

  block_id_t next_hint_id = 0;

  for(const auto& [from, to]: edges_to_split) {
    BasicBlockPack bbp;
    bbp.label = Label{LabelHint::kCriticalEdge, next_hint_id++};
    BranchInst lineB;
    lineB.label = func.basic_block_packs[to].label;
    bbp.instructions.emplace_back(std::make_unique<BranchInst>(std::move(lineB)));
    func.basic_block_packs.push_back(std::move(bbp));

    for(auto& inst: func.basic_block_packs[from].instructions) {
      if(auto* br = dynamic_cast<BranchInst*>(inst.get())) {
        if(br->label == lineB.label) br->label = bbp.label;
      } else if(auto* cond = dynamic_cast<CondBranchInst*>(inst.get())) {
        if(cond->true_label == lineB.label) cond->true_label = bbp.label;
        if(cond->false_label == lineB.label) cond->false_label = bbp.label;
      }
    }

    for(auto& inst: func.basic_block_packs[to].instructions) {
      if(auto* phi = dynamic_cast<PhiInst*>(inst.get())) {
        for(auto &[op, label]: phi->incoming) {
          if(label == func.basic_block_packs[from].label) {
            label = bbp.label;
          }
        }
      }
    }
  }
  func.update_block_ids();
}

void Canonicalization::optimize(FunctionPack& func) {
  func.update_block_ids();
  func.reorder_reg_ids();
  func.instr_renumbering();

  eliminate_single_phi(func);
  eliminate_critical_edge(func);
  while(eliminate_deadcode(func)) {
    /* loop */
  }
  while(constant_fold(func)) {
    /* loop */
  }
  func.reorder_reg_ids();
  func.instr_renumbering();
}

std::optional<std::int32_t> instr_cost(const Instruction* inst, const StringT& func_name) {
  static constexpr std::int32_t COST_ALLOC = 1;
  static constexpr std::int32_t COST_BASIC_BINOP = 1;
  static constexpr std::int32_t COST_MUL = 2;
  static constexpr std::int32_t COST_DIV = 10;
  static constexpr std::int32_t COST_REM = 10;
  static constexpr std::int32_t COST_LOAD_STORE = 3;
  static constexpr std::int32_t COST_BRANCH = 1;
  static constexpr std::int32_t COST_CALL = 20;
  static constexpr std::int32_t COST_INSERT_VALUE = 3;
  static constexpr std::int32_t COST_GEP = 3;
  static constexpr std::int32_t COST_CAST = 1;
  static constexpr std::int32_t COST_PHI = 1;
  static constexpr std::int32_t COST_RETURN = 1;
  static constexpr std::int32_t COST_UNREACHABLE = 0;

  if(auto a = dynamic_cast<const AllocaInst*>(inst)) {
    return COST_ALLOC;
  }
  if(const auto s = dynamic_cast<const StoreInst*>(inst)) {
    return COST_LOAD_STORE;
  }
  if(const auto l = dynamic_cast<const LoadInst*>(inst)) {
    return COST_LOAD_STORE;
  }
  if(const auto bi = dynamic_cast<const BinaryOpInst*>(inst)) {
    if(bi->op == "mul") return COST_MUL;
    if(bi->op == "sdiv" || bi->op == "udiv") return COST_DIV;
    if(bi->op == "srem" || bi->op == "urem") return COST_REM;
    return COST_BASIC_BINOP;
  }
  if(const auto call = dynamic_cast<const CallInst*>(inst)) {
    if(call->func_name == func_name) return std::nullopt;
    return COST_CALL;
  }
  if(const auto r = dynamic_cast<const ReturnInst*>(inst)) {
    return COST_RETURN;
  }
  if(const auto g = dynamic_cast<const GEPInst*>(inst)) {
    return COST_GEP;
  }
  if(const auto cast = dynamic_cast<const CastInst*>(inst)) {
    return COST_CAST;
  }
  if(const auto br = dynamic_cast<const BranchInst*>(inst)) {
    return COST_BRANCH;
  }
  if(const auto cond = dynamic_cast<const CondBranchInst*>(inst)) {
    return COST_BRANCH;
  }
  if(const auto u = dynamic_cast<const UnreachableInst*>(inst)) {
    return COST_UNREACHABLE;
  }
  if(const auto i = dynamic_cast<const InsertValueInst*>(inst)) {
    return COST_INSERT_VALUE;
  }
  if(const auto p = dynamic_cast<const PhiInst*>(inst)) {
    return COST_PHI;
  }

  throw std::runtime_error("Unknown instruction");
}

std::optional<std::int32_t> func_cost(const FunctionPack& func) {
  std::int32_t res = 0;
  for(const auto &block: func.basic_block_packs) {
    for(const auto &inst: block.instructions) {
      auto cost = instr_cost(inst.get(), func.ident);
      if(!cost) return std::nullopt;
      res += *cost;
    }
  }
  return {res};
}

std::optional<FunctionPack> extract_cheap_func(IrPack& ir) {
  static constexpr std::int32_t THRESHOLD = 40;
  for(auto it = ir.function_packs.begin(); it != ir.function_packs.end(); ++it) {
    auto cost = func_cost(*it);
    if(cost && *cost <= THRESHOLD) {
      std::optional res = std::move(*it);
      ir.function_packs.erase(it);
      return res;
    }
  }
  return std::nullopt;
}

void inline_func(IrPack& ir, FunctionPack&& cheap_func) {
  auto cheap_name = cheap_func.ident;
  for(auto& func: ir.function_packs) {
    hint_id_t hint_id = -1;
    for(int idx_bb = 0; idx_bb < func.basic_block_packs.size(); ++idx_bb) {
      for(auto idx_inst = 0; idx_inst < func.basic_block_packs[idx_bb].instructions.size(); ++idx_inst) {
        auto it_inst = func.basic_block_packs[idx_bb].instructions.begin() + idx_inst;
        if(auto call = dynamic_cast<CallInst*>(it_inst->get()); call && call->func_name == cheap_name) {
          const reg_id_t reg_id_offset = func.largest_reg_id() + 1;
          auto it_bb = func.basic_block_packs.begin() + idx_bb;
          std::vector<PhiInst::Income> incomes;
          Label starting_label = it_bb->label;
          Label ending_label{LabelHint::kInlineExit, ++hint_id};
          ending_label.appendix.emplace_back(cheap_name, hint_id);
          BasicBlockPack left;
          left.label = it_bb->label;
          for(int i = 0; i < idx_inst; ++i) {
            left.instructions.push_back(std::move(it_bb->instructions[i]));
          }
          BasicBlockPack right;
          right.label = ending_label;
          for(int i = idx_inst + 1; i < it_bb->instructions.size(); ++i) {
            right.instructions.push_back(std::move(it_bb->instructions[i]));
          }
          std::unordered_map<reg_id_t, Operand> param_reg_map;
          for(int i = 0; i < call->args.size(); ++i) {
            param_reg_map.emplace(cheap_func.param_at(i).as_reg(), call->args[i]);
          }
          std::vector<BasicBlockPack> new_packs;
          // deep copy
          for(auto& bb: cheap_func.basic_block_packs) {
            BasicBlockPack new_bb;
            new_bb.label = bb.label;
            for(auto& inst: bb.instructions) {
              new_bb.instructions.emplace_back(inst->clone()); // no moving!
            }
            new_packs.push_back(std::move(new_bb));
          }
          BasicBlockPack term_block;
          // rename all blocks + regs
          for(auto& nbb: new_packs) {
            nbb.label.appendix.emplace_back(cheap_name, hint_id);
            for(auto& ninst: nbb.instructions) {
              if(auto dst = ninst->get_dst()) {
                ninst->set_dst(*dst + reg_id_offset);
              }
              for(auto *use: ninst->get_uses_oper()) {
                if(use->is_reg()) {
                  if(param_reg_map.contains(use->as_reg())) {
                    *use = param_reg_map.at(use->as_reg());
                  } else {
                    use->value = Operand::VRegister{use->as_reg() + reg_id_offset};
                  }
                }
              }

              if(auto *br = dynamic_cast<BranchInst*>(ninst.get())) {
                br->label.appendix.emplace_back(cheap_name, hint_id);
              } else if(auto *cbr = dynamic_cast<CondBranchInst*>(ninst.get())) {
                cbr->true_label.appendix.emplace_back(cheap_name, hint_id);
                cbr->false_label.appendix.emplace_back(cheap_name, hint_id);
              } else if(auto *phi = dynamic_cast<PhiInst*>(ninst.get())) {
                for(auto& income: phi->incoming) {
                  income.label.appendix.emplace_back(cheap_name, hint_id);
                }
              }

              if(auto* ret = dynamic_cast<ReturnInst*>(ninst.get())) {
                if(ret->ret_val) incomes.emplace_back(*ret->ret_val, nbb.label);
                BranchInst lineB;
                lineB.label = ending_label;
                ninst = std::make_unique<BranchInst>(std::move(lineB));
              }
            }
          }

          BranchInst lineB;
          lineB.label = new_packs.front().label;
          left.instructions.push_back(std::make_unique<BranchInst>(std::move(lineB)));
          if(call->dst) {
            PhiInst lineP;
            lineP.dst = *call->dst;
            lineP.type = call->ret_type;
            lineP.incoming = std::move(incomes);
            right.instructions.insert(right.instructions.begin(), std::make_unique<PhiInst>(std::move(lineP)));
          }
          new_packs.insert(new_packs.begin(), std::move(left));
          new_packs.push_back(std::move(right));

          // it_bb and &bb not used here. might become invalid
          func.basic_block_packs.erase(func.basic_block_packs.begin() + idx_bb);
          for(auto& pack: new_packs) {
            func.basic_block_packs.insert(func.basic_block_packs.begin() + (idx_bb++), std::move(pack));
          }

          // replace the phi incoming labels.
          // Those who comes from the starting label (original one) shall be redirected to ending label.
          for(auto &bb: func.basic_block_packs) {
            for(auto &inst: bb.instructions) {
              if(auto *phi = dynamic_cast<PhiInst*>(inst.get())) {
                for(auto &income: phi->incoming) {
                  if(income.label == starting_label)
                    income.label = ending_label;
                }
              }
            }
          }

          idx_bb -= 2; break; // go to the `right` bb defined above and start from the first instruction
        }
      }
    }
    if(hint_id >= 0) {
      func.update_block_ids();
      func.cfg.valid = false;
    }
  }
}

void FunctionInline::optimize(IrPack& ir) {
  while(auto cheap_func = extract_cheap_func(ir)) {
    inline_func(ir, std::move(*cheap_func));
    // for(auto& func: ir.function_packs) {
    //   while(constant_fold(func)) { /* */ }
    // }
  }
  Canonicalization::optimize(ir);
}

std::unordered_map<reg_id_t, IrType> find_promotable_slots(FunctionPack& func) {
  std::unordered_map<reg_id_t, IrType> alloca_slots;

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      if(auto* a = dynamic_cast<AllocaInst*>(inst.get())) {
        alloca_slots.emplace(a->dst, a->type);
      }
    }
  }

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      if(dynamic_cast<AllocaInst*>(inst.get())) continue;
      if(dynamic_cast<LoadInst*>(inst.get())) continue;
      if(dynamic_cast<StoreInst*>(inst.get())) continue;

      for(reg_id_t use: inst->get_uses()) {
        if(alloca_slots.contains(use)) {
          alloca_slots.erase(use);
        }
      }
    }
  }

  return alloca_slots;
}

// all blocks used this reg slot as StoreInst destination.
std::vector<block_id_t> collect_def_blocks(FunctionPack& func, reg_id_t slot) {
  std::unordered_set<block_id_t> blocks;
  for(block_id_t i = 0; i < func.basic_block_packs.size(); ++i) {
    for(auto& inst: func.basic_block_packs[i].instructions) {
      if(auto* s = dynamic_cast<StoreInst*>(inst.get())) {
        if(s->ptr.is_reg() && s->ptr.as_reg() == slot) {
          blocks.insert(i);
        }
      }
    }
  }
  return {blocks.begin(), blocks.end()};
}

bool has_phi_for_slot(FunctionPack& func, int block_id, reg_id_t slot) {
  for(auto& inst: func.basic_block_packs[block_id].instructions) {
    if(auto* phi = dynamic_cast<PhiInst*>(inst.get())) {
      if(phi->original_slot == slot) return true;
    } else break;
  }
  return false;
}

void insert_phi_for_slot(FunctionPack& func, reg_id_t slot, IrType slot_type) {
  auto worklist = collect_def_blocks(func, slot);
  if(worklist.empty()) return;

  std::unordered_set<block_id_t> visited(worklist.begin(), worklist.end());

  for(size_t i = 0; i < worklist.size(); ++i) {
    int b = worklist[i];
    for(int df_block: func.dom_tree.df[b]) {
      if(!has_phi_for_slot(func, df_block, slot)) {
        auto phi = std::make_unique<PhiInst>();
        phi->original_slot = slot;
        phi->dst = -3; // change later
        phi->type = slot_type;
        // phi->incoming is blank now

        auto& instrs = func.basic_block_packs[df_block].instructions;
        auto it = std::find_if(instrs.begin(), instrs.end(),
                               [](const std::unique_ptr<Instruction>& instr) {
                                 return !dynamic_cast<PhiInst*>(instr.get());
                               });
        instrs.insert(it, std::move(phi));

        if(visited.insert(df_block).second) {
          worklist.push_back(df_block);
        }
      }
    }
  }
}

Operand check_use(Operand use, const std::unordered_map<reg_id_t, Operand>& use_replacement) {
  while(use.is_reg() && use_replacement.contains(use.as_reg())) {
    use = use_replacement.at(use.as_reg());
  }
  return use;
}

struct SlotRenamer {
  FunctionPack& func;
  std::unordered_map<reg_id_t, Operand>& use_replacement;
  std::unordered_map<reg_id_t, std::stack<Operand>> version_stacks;
  reg_id_t next_version; // next SSA reg.

  SlotRenamer(FunctionPack& f, const std::unordered_map<reg_id_t, IrType>& promotable,
    reg_id_t next_ver, std::unordered_map<reg_id_t, Operand>& replacement)
    : func(f), use_replacement(replacement), next_version(next_ver) {
    for(auto& [slot, _tp]: promotable) version_stacks.emplace(slot, std::stack<Operand>{});
  }

  void push(reg_id_t slot, const Operand& oper) { version_stacks.at(slot).push(oper); }
  void pop(reg_id_t slot) {
    if(version_stacks.at(slot).empty()) throw std::runtime_error("Unexpected behaviour");
    version_stacks.at(slot).pop();
  }
  [[nodiscard]] Operand current(reg_id_t slot) const {
    if(version_stacks.at(slot).empty()) throw std::runtime_error("Unexpected behaviour");
    return version_stacks.at(slot).top();
  }
  reg_id_t new_version() { return next_version++; }

  void rename(int block_id) {
    auto& block = func.basic_block_packs[block_id];

    // number of versions recorded in this block
    std::unordered_map<reg_id_t, int> push_counts; // initially zero

    // PhiInst need new names
    for(auto& inst: block.instructions) {
      if(auto* phi = dynamic_cast<PhiInst*>(inst.get())) {
        if(!version_stacks.contains(phi->original_slot)) continue;

        reg_id_t ver = new_version();
        phi->dst = ver;
        push(phi->original_slot, Operand::make_reg(ver, {}));
        push_counts[phi->original_slot]++;
      } else break; // no need to check others.
    }

    for(auto& inst: block.instructions) {
      // PhiInst preserved.
      if(dynamic_cast<PhiInst*>(inst.get())) {
        continue;
      }

      if(auto* store = dynamic_cast<StoreInst*>(inst.get())) {
        if(store->ptr.is_reg() && version_stacks.contains(store->ptr.as_reg())) {
          push(store->ptr.as_reg(), check_use(store->value, use_replacement));
          push_counts[store->ptr.as_reg()]++;
          // StoreInst erased.
          inst.reset();
          continue;
        }
      }

      if(auto* load = dynamic_cast<LoadInst*>(inst.get())) {
        if(load->ptr.is_reg() && version_stacks.contains(load->ptr.as_reg())) {
          use_replacement.emplace(load->dst, current(load->ptr.as_reg()));
          // LoadInst erased.
          inst.reset();
          continue;
        }
      }
    }

    // add PhiInst for successor blocks.
    for(int succ: func.cfg.succ[block_id]) {
      for(auto& inst: func.basic_block_packs[succ].instructions) {
        auto* phi = dynamic_cast<PhiInst*>(inst.get());
        if(!phi) break;

        if(!version_stacks.contains(phi->original_slot)) continue;
        if(version_stacks.at(phi->original_slot).empty()) continue; // not defined yet

        bool found = false;
        for(auto& [_op, label]: phi->incoming) {
          if(label.block_id == block_id) {
            found = true;
            break;
          }
        }
        if(!found) {
          // Add incoming using current version
          phi->incoming.emplace_back(current(phi->original_slot), block.label);
        }
      }
    }

    for(int child: func.dom_tree.children[block_id]) {
      rename(child);
    }

    // rollback
    for(auto& [slot, cnt]: push_counts) {
      while(cnt--) pop(slot);
    }
  }
};

void PromoteAlloca::optimize(FunctionPack& func) {
  func.construct_domtree();

  auto promotable = find_promotable_slots(func);

  // I'm lazy to cache it.
  reg_id_t max_reg = 0;
  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      if(auto dst = inst->get_dst()) {
        max_reg = std::max(max_reg, *dst);
      }
      for(auto use: inst->get_uses()) {
        max_reg = std::max(max_reg, use);
      }
    }
  }

  std::unordered_map<reg_id_t, Operand> use_replacement;

  // (SSA) PromoteAlloca process.
  for(auto& [slot, slot_type]: promotable) {
    insert_phi_for_slot(func, slot, slot_type);
  }
  SlotRenamer{func, promotable, max_reg + 1, use_replacement}.rename(0);

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      if(!inst) continue;
      for(auto& use: inst->get_uses()) {
        if(use_replacement.contains(use)) {
          inst->replace_use(use, check_use(use_replacement.at(use), use_replacement));
        }
      }
    }
  }

  for(auto& bb: func.basic_block_packs) {
    auto empty_checker = [&promotable](const std::unique_ptr<Instruction>& inst) {
      if(!inst) return true;
      if(auto* a = dynamic_cast<AllocaInst*>(inst.get())) {
        return promotable.contains(a->dst);
      }
      return false;
    };
    std::erase_if(bb.instructions, empty_checker);
  }
}
}
