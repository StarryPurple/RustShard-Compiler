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
  return std::vector<block_id_t>(blocks.begin(), blocks.end());
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
  const reg_id_t slot;
  std::unordered_map<reg_id_t, Operand>& use_replacement;

  std::stack<Operand> version_stack; // oper type is not important.
  // next SSA reg.
  reg_id_t next_version;

  SlotRenamer(FunctionPack& f, reg_id_t s, reg_id_t next_ver, std::unordered_map<reg_id_t, Operand>& replacement)
    : func(f), slot(s), next_version(next_ver), use_replacement(replacement) {}

  void push(const Operand& oper) { version_stack.push(oper); }
  void pop() {
    if(version_stack.empty()) throw std::runtime_error("Unexpected behaviour");
    version_stack.pop();
  }
  [[nodiscard]] Operand current() const {
    if(version_stack.empty()) throw std::runtime_error("Unexpected behaviour");
    return version_stack.top();
  }
  reg_id_t new_version() { return next_version++; }

  void rename(int block_id) {
    auto& block = func.basic_block_packs[block_id];

    // number of versions recorded in this block
    int pushed_count = 0;

    // PhiInst need new names
    for(auto& inst: block.instructions) {
      if(auto* phi = dynamic_cast<PhiInst*>(inst.get())) {
        if(phi->original_slot != slot) continue;

        reg_id_t ver = new_version();
        phi->dst = ver;
        push(Operand::make_reg(ver, {}));
        pushed_count++;
      } else break; // no need to check others.
    }

    for(auto& inst: block.instructions) {
      // PhiInst preserved.
      if(dynamic_cast<PhiInst*>(inst.get())) {
        continue;
      }

      if(auto* store = dynamic_cast<StoreInst*>(inst.get())) {
        if(store->ptr.is_reg() && store->ptr.as_reg() == slot) {
          push(check_use(store->value, use_replacement));
          pushed_count++;
          // StoreInst erased.
          inst.reset();
          continue;
        }
      }

      if(auto* load = dynamic_cast<LoadInst*>(inst.get())) {
        if(load->ptr.is_reg() && load->ptr.as_reg() == slot) {
          use_replacement.emplace(load->dst, current());
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

        if(phi->original_slot != slot) continue;
        if(version_stack.empty()) continue; // not defined yet

        bool found = false;
        for(auto& [_op, label]: phi->incoming) {
          if(label.block_id == block_id) {
            found = true;
            break;
          }
        }
        if(!found) {
          // Add incoming using current version
          phi->incoming.emplace_back(current(), block.label);
        }
      }
    }

    for(int child: func.dom_tree.children[block_id]) {
      rename(child);
    }

    // rollback
    while(pushed_count--) pop();
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
  reg_id_t next_ssa_reg = max_reg + 1;
  std::unordered_map<reg_id_t, Operand> use_replacement;

  // (SSA) PromoteAlloca process.
  for(auto& [slot, slot_type]: promotable) {
    insert_phi_for_slot(func, slot, slot_type);

    SlotRenamer renamer(func, slot, next_ssa_reg, use_replacement);

    // (undefined and never used) initial value of the slot.
    renamer.push(Operand::make_reg(-3, {}));

    renamer.rename(0); // entry block

    next_ssa_reg = renamer.next_version;
  }

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
