#include "backend/regalloc.hpp"

#include <algorithm>
#include <format>
#include <stack>

namespace rshard::backend {
LivenessInfo compute_liveness(const ir::FunctionPack& func) {
  LivenessInfo info;

  if(!func.cfg.valid) {
    const_cast<ir::FunctionPack&>(func).construct_cfg();
  }

  std::unordered_set<ir::reg_id_t> alloca_defs;
  for(const auto& bb: func.basic_block_packs) {
    auto block_id = bb.label.block_id;
    auto& def_set = info.def[block_id];
    auto& use_set = info.use[block_id];
    std::unordered_set<ir::reg_id_t> locally_defined;

    for(const auto& inst: bb.instructions) {
      if(auto* alloc = dynamic_cast<const ir::AllocaInst*>(inst.get())) {
        // alloca inst results do not need physical registers.
        // They'll be calculated by immediate offset.
        alloca_defs.insert(alloc->dst);
        continue;
      }
      if(auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get())) {
        def_set.insert(phi->dst);
        locally_defined.insert(phi->dst);
        continue;
      }

      for(auto u: inst->get_uses()) {
        if(!locally_defined.contains(u) && !alloca_defs.contains(u)) {
          use_set.insert(u);
        }
      }
      if(auto dst = inst->get_dst()) {
        locally_defined.insert(*dst);
        def_set.insert(*dst);
      }
    }
  }

  bool changed = true;
  while(changed) {
    changed = false;

    for(const auto& bb: func.basic_block_packs) {
      auto block_id = bb.label.block_id;

      std::unordered_set<ir::reg_id_t> new_live_out;
      for(auto succ: func.cfg.succ[block_id]) {
        for(auto reg: info.live_in[succ]) new_live_out.insert(reg);
        for(const auto& inst: func.basic_block_packs[succ].instructions) {
          auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get());
          if(!phi) break;
          for(auto& [op, label]: phi->incoming) {
            if(label.block_id == block_id && op.is_reg()) {
              new_live_out.insert(op.as_reg());
            }
          }
        }
      }

      if(info.live_out[block_id] != new_live_out) {
        info.live_out[block_id] = std::move(new_live_out);
        changed = true;
      }

      auto new_live_in = info.live_out[block_id];
      for(auto d: info.def[block_id]) new_live_in.erase(d);
      for(auto u: info.use[block_id]) new_live_in.insert(u);

      if(info.live_in[block_id] != new_live_in) {
        info.live_in[block_id] = std::move(new_live_in);
        changed = true;
      }
    }
  }

  return info;
}

std::vector<LiveInterval> build_intervals(const ir::FunctionPack& func) {
  std::unordered_map<ir::reg_id_t, ir::instr_no_t> first_def_global;
  std::unordered_map<ir::reg_id_t, ir::instr_no_t> last_use_global;
  std::unordered_map<ir::reg_id_t, std::int32_t> use_counts;

  // An instr_renumbering is conducted here, by the way.

  if(func.sret_param) {
    first_def_global[func.sret_param->as_reg()] = 0;
  }
  for(auto& param: func.params) {
    first_def_global[param.as_reg()] = 0;
  }
  ir::instr_no_t cnt = 0;
  for(auto& bb: func.basic_block_packs) {
    auto block_id = bb.label.block_id;
    for(auto& inst: bb.instructions) {
      inst->instr_no = ++cnt;
      if(auto dst = inst->get_dst()) {
        if(!dynamic_cast<ir::AllocaInst*>(inst.get())) {
          // AllocaInst dst do not need a reg
          if(!first_def_global.contains(*dst)) {
            first_def_global[*dst] = inst->instr_no; // + (dynamic_cast<ir::CallInst*>(inst.get()) ? 1 : 0);
          }
        }
      }
      if(!dynamic_cast<ir::PhiInst*>(inst.get())) {
        // PhiInst incoming reg use do not reach this inst itself
        for(auto use: inst->get_uses()) {
          last_use_global[use] = inst->instr_no;
          use_counts[use]++;
        }
      }
    }
  }

  auto liveness = compute_liveness(func);
  for(const auto& bb: func.basic_block_packs) {
    auto block_id = bb.label.block_id;
    ir::instr_no_t last_instr = bb.instructions.back()->instr_no;
    for(auto reg: liveness.live_out.at(block_id)) {
      last_use_global[reg] = std::max(last_use_global[reg], last_instr);
    }
  }

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      auto* phi = dynamic_cast<ir::PhiInst*>(inst.get());
      if(!phi) continue;
      for(const auto& [op, label]: phi->incoming) {
        if(op.is_reg()) {
          auto& block = func.basic_block_packs[label.block_id];
          last_use_global[op.as_reg()] = std::max(last_use_global[op.as_reg()], block.instructions.back()->instr_no);
        }
      }
    }
  }

  std::unordered_set<ir::reg_id_t> all_regs;
  for(const auto& [reg, _]: first_def_global) all_regs.insert(reg);

  std::vector<LiveInterval> intervals;
  for(auto reg: all_regs) {
    if(reg >= 8 && reg < func.param_num()) continue; // no need
    ir::instr_no_t start = first_def_global.at(reg);
    ir::instr_no_t end = last_use_global.count(reg) ? last_use_global.at(reg) : start;
    intervals.push_back(LiveInterval{reg, start, end, use_counts[reg]});
  }

  std::ranges::sort(intervals, [](const LiveInterval& lhs, const LiveInterval& rhs) {
    return lhs.start == rhs.start ? lhs.end < rhs.end : lhs.start < rhs.start;
  });

  return intervals;
}

void linear_coloring(const ir::FunctionPack& func, const std::vector<LiveInterval>& intervals,
                     AllocationResult& result) {
  std::vector reg_pool(kAllocatableRegs.begin(), kAllocatableRegs.end());

  std::unordered_map<ir::reg_id_t, LiveInterval> ints_map;
  for(auto& interval: intervals) ints_map.emplace(interval.reg, interval);

  std::unordered_map<PhysReg, LiveInterval> active;

  // function params 8- cannot be spilled (otherwise to be dealt outside this function)
  for(int i = 0; i < func.param_num() && i < 8; ++i) {
    auto pr = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + i);
    result.mapping.emplace(func.param_at(i).as_reg(), Location::make_reg(pr));
    active.emplace(pr, ints_map.at(i));
  }

  size_t spill_area_size = 0;

  // Linear coloring
  for(const auto& interval: intervals) {
    if(interval.reg < func.param_num()) continue;
    if(result.mapping.contains(interval.reg)) continue;

    std::erase_if(active, [&](const auto& kv) {
      return kv.second.end < interval.start;
    });

    bool allocated = false;
    for(auto pr: reg_pool) {
      if(!active.contains(pr)) {
        result.mapping.emplace(interval.reg, Location::make_reg(pr));
        active.emplace(pr, interval);
        allocated = true;
        break;
      }
    }

    if(allocated) continue;

    auto spill_it = std::ranges::min_element(
      active, [](const auto& a, const auto& b) { return a.second.weight() < b.second.weight(); });

    if(spill_it != active.end() && spill_it->second.weight() < interval.weight()) {
      result.mapping[spill_it->second.reg] = Location::make_spill(spill_area_size);
      spill_area_size += 8;
      PhysReg freed_reg = spill_it->first;
      active.erase(spill_it);
      result.mapping.emplace(interval.reg, Location::make_reg(freed_reg));
      active.emplace(freed_reg, interval);
    } else {
      result.mapping.emplace(interval.reg, Location::make_spill(spill_area_size));
      spill_area_size += 8;
    }
  }
  result.spill_area_size = spill_area_size;
}

void graph_coloring(const ir::FunctionPack& func, const std::vector<LiveInterval>& intervals,
                     AllocationResult& result) {
  const auto K = kAllocatableRegs.size();
  const int vregNum = intervals.size();
  const int vregSup = func.largest_reg_id() + 1;

  std::vector<std::optional<std::unordered_set<ir::reg_id_t>>> adj;
  adj.resize(vregSup, std::nullopt);
  for(auto& iv: intervals) {
    if(iv.reg >= 8 && iv.reg < func.param_num()) continue;
    adj[iv.reg] = std::unordered_set<ir::reg_id_t>{};
  }

  for(int i = 0; i < vregNum; ++i) {
    for(int j = i + 1; j < vregNum; ++j) {
      if(intervals[i].start <= intervals[j].end && intervals[j].start <= intervals[i].end) {
        adj[intervals[i].reg]->emplace(intervals[j].reg);
        adj[intervals[j].reg]->emplace(intervals[i].reg);
      }
    }
  }

  std::unordered_map<ir::reg_id_t, PhysReg> hints;
  for(int i = 0; i < func.param_num() && i < 8; ++i) {
    hints.emplace(func.param_at(i).as_reg(), static_cast<PhysReg>(static_cast<int>(PhysReg::a0) + i));
  }

  auto adj_rep = adj;
  auto adj_backup = adj;
  int count = 0;
  std::size_t spill_area_size = 0;
  while(true) {
    bool full = true;
    for(int i = 0; i < vregNum; ++i) {
      auto vi = intervals[i].reg;
      if(adj_rep[vi] && adj_rep[vi]->size() < K) {
        for(auto& a: adj_rep) if(a) a->erase(vi);
        adj_rep[vi] = std::nullopt;
        count++;
        full = false;
      }
    }
    if(full) {
      if(count == vregNum) break; // alloc finished
      // still something need to be evicted.
      // choose the one with the minimum weight.
      std::int32_t min_weight = INT32_MAX;
      int idx = -1;
      for(int i = 0; i < vregNum; ++i) {
        auto vi = intervals[i].reg;
        if(adj_rep[vi]) {
          auto weight = intervals[i].weight();
          if(weight < min_weight) {
            idx = vi;
            min_weight = weight;
          }
        }
      }

      for(auto& a: adj_rep) if(a) a->erase(idx);
      for(auto& a: adj) if(a) a->erase(idx);
      adj_rep[idx] = std::nullopt;
      adj[idx] = std::nullopt;
      result.mapping[idx] = Location::make_spill(spill_area_size);
      spill_area_size += 8;
      count++;
    }
  }
  result.spill_area_size = spill_area_size;

  std::vector<std::optional<PhysReg>> reg_map;
  reg_map.resize(vregSup);
  // coloring
  bool color[32];
  for(int i = 0; i < vregSup; ++i) {
    if(adj[i]) {
      memset(color, 0, sizeof(color));
      for(auto j: *adj[i]) if(auto col = reg_map[j]) {
        color[static_cast<int>(*col)] = true;
      }
      if(hints.contains(i) && !color[static_cast<int>(hints[i])]) {
        reg_map[i] = std::optional{hints[i]};
      } else {
        for(auto& pr: kAllocatableRegs) if(!color[static_cast<int>(pr)]) {
          reg_map[i] = std::optional{pr};
          break;
        }
      }
      result.mapping[i] = Location::make_reg(*reg_map[i]);
    }
  }

  /*
  std::cout << func.ident << '\n';
  for(int i = 0; i < vregSup; ++i) if(adj[i]) {
    std::vector<int> buf {adj[i]->begin(), adj[i]->end()};
    std::sort(buf.begin(), buf.end());
    std::cout << std::format("vreg {} adj: ", i);
    for(auto& v: buf) {
      std::cout << v << ' ';
      if(result.mapping[i] == result.mapping[v]) {
        throw std::runtime_error("Reg alloc: same mapping");
      }
    }
    std::cout << '\n';
  }
  */
}

std::unordered_map<ir::reg_id_t, std::size_t> local_var_alloc(const ir::FunctionPack& func, AllocationResult& result) {
  std::unordered_map<ir::reg_id_t, std::size_t> local_var_mapping;
  std::size_t local_var_size = 0;
  for(const auto& bb: func.basic_block_packs) {
    for(const auto& inst: bb.instructions) {
      if(auto* alloca = dynamic_cast<const ir::AllocaInst*>(inst.get())) {
        size_t size = alloca->type.size();
        size_t align = alloca->type.align();
        local_var_size = (local_var_size + align - 1) & ~(align - 1);
        // not here.
        // result.mapping[alloca->dst] = Location::make_addr(local_var_size);
        local_var_mapping.emplace(alloca->dst, local_var_size);
        local_var_size += size;
      }
    }
  }
  result.local_var_size = local_var_size;
  return local_var_mapping;
}

void calc_caller_callee_save(const ir::FunctionPack& func, AllocationResult& result,
                             const std::vector<LiveInterval>& intervals) {
  for(auto& [vreg, loc]: result.mapping) {
    if(loc.is_reg() && kCalleeSaveRegs.contains(loc.as_reg())) {
      result.callee_saved_used.insert(loc.as_reg());
    }
    if(loc.is_reg() && kCallerSaveRegs.contains(loc.as_reg())) {
      result.caller_saved_used.insert(loc.as_reg());
    }
  }

  for(const auto& interval: intervals) {
    auto it = result.mapping.find(interval.reg);
    if(it == result.mapping.end() || !it->second.is_reg()) continue;
    result.preg_interval[it->second.as_reg()].push_back(interval);
  }
  for(auto& [vreg, ints]: result.preg_interval) {
    std::ranges::sort(ints, [](const LiveInterval& lhs, const LiveInterval& rhs) {
      return lhs.start < rhs.start;
    });
  }

  for(auto& bb: func.basic_block_packs) {
    for(auto& inst: bb.instructions) {
      const auto* call = dynamic_cast<ir::CallInst*>(inst.get());
      if(!call) continue;
      result.caller_to_save.emplace(call, std::unordered_set<PhysReg>{});
      for(auto pr: kCallerSaveRegs) {
        auto& ints = result.preg_interval[pr];
        auto it = std::upper_bound(ints.begin(), ints.end(), call->instr_no,
                                   [](ir::instr_no_t instr_no, const LiveInterval& interval) {
                                     return instr_no < interval.start;
                                   });
        if(it != ints.begin()) {
          --it;
          if(it->end >= call->instr_no) {
            result.caller_to_save[call].insert(pr);
          }
        }
      }
    }
  }

  std::size_t caller_save_num = 0;
  for(auto& [call_inst, saves]: result.caller_to_save) {
    caller_save_num = std::max(caller_save_num, saves.size());
  }
  result.max_caller_save_num = caller_save_num;
}

AllocationResult allocate_registers(const ir::FunctionPack& func) {
  AllocationResult result;

  auto intervals = build_intervals(func);
  linear_coloring(func, intervals, result);
  // graph_coloring(func, intervals, result);
  auto local_var_mapping = local_var_alloc(func, result);
  calc_caller_callee_save(func, result, intervals);

  result.spill_args_num = (func.param_num() > 8) ? (func.param_num() - 8) : 0;
  result.total_frame_size
    = result.spill_area_size // virtual reg spill
    + result.local_var_size // local Alloca region
    + result.callee_saved_used.size() * 8 // call of self: saving some callee-saved regs
    + result.max_caller_save_num * 8 // call of other function: saving some caller-saved regs
    + (result.caller_to_save.empty() ? 0 : 8) // save ra
    + result.spill_args_num * 8; // args passed on stack
  result.total_frame_size = (result.total_frame_size + 15) & ~15ul; // 16-byte alignment req of $sp

  // function param spill

  offset_t args_start = result.spill_args_offset();
  for(int i = 8; i < func.param_num(); ++i) {
    result.mapping.emplace(func.param_at(i).as_reg(), Location::make_spill(args_start + 8 * (i - 8)));
  }

  for(const auto& [vreg, offset]: local_var_mapping) {
    result.mapping.emplace(vreg, Location::make_addr(result.local_var_offset() + offset));
  }

  /*
  std::cout << func.ident << ":\n";
  for(auto& iv: intervals) {
    std::cout << std::format("interval {}: {} - {}, count: {}, ", iv.reg, iv.start, iv.end, iv.use_count);
    auto loc = result.mapping.at(iv.reg);
    std::string loc_str;
    if(loc.is_imm()) loc_str = "imm: " + std::to_string(loc.as_imm());
    if(loc.is_reg()) loc_str = "reg: " + kRegNames.at(loc.as_reg());
    if(loc.is_addr()) loc_str = "addr: " + std::to_string(loc.as_addr());
    if(loc.is_spill()) loc_str = "spill: " + std::to_string(loc.as_spill());
    std::cout << loc_str << '\n';
  }
  */

  return result;
}
} // namespace rshard::backend
