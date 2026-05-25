#include "backend/regalloc.hpp"

namespace rshard::backend {

LivenessInfo compute_liveness(const ir::FunctionPack& func) {
  LivenessInfo info;

  if (!func.cfg.valid) {
    const_cast<ir::FunctionPack&>(func).construct_cfg();
  }

  for (const auto& bb : func.basic_block_packs) {
    auto block_id = bb.label.block_id;
    auto& def_set = info.def[block_id];
    auto& use_set = info.use[block_id];
    std::unordered_set<ir::reg_id_t> locally_defined;

    for (const auto& inst : bb.instructions) {
      if (auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get())) {
        def_set.insert(phi->dst);
        locally_defined.insert(phi->dst);
        continue;
      }

      for (auto u : inst->get_uses()) {
        if (!locally_defined.count(u)) use_set.insert(u);
      }

      if (auto dst = inst->get_dst()) {
        locally_defined.insert(*dst);
        def_set.insert(*dst);
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;

    for (const auto& bb : func.basic_block_packs) {
      auto block_id = bb.label.block_id;

      std::unordered_set<ir::reg_id_t> new_live_out;
      for (auto succ : func.cfg.succ[block_id]) {
        for (auto reg : info.live_in[succ]) new_live_out.insert(reg);
        for (const auto& inst : func.basic_block_packs[succ].instructions) {
          auto* phi = dynamic_cast<const ir::PhiInst*>(inst.get());
          if (!phi) break;
          for (auto& [op, label] : phi->incoming) {
            if (label.block_id == block_id && op.is_reg()) {
              new_live_out.insert(op.as_reg());
            }
          }
        }
      }

      if (info.live_out[block_id] != new_live_out) {
        info.live_out[block_id] = std::move(new_live_out);
        changed = true;
      }

      auto new_live_in = info.live_out[block_id];
      for (auto d : info.def[block_id]) new_live_in.erase(d);
      for (auto u : info.use[block_id]) new_live_in.insert(u);

      if (info.live_in[block_id] != new_live_in) {
        info.live_in[block_id] = std::move(new_live_in);
        changed = true;
      }
    }
  }

  return info;
}

namespace {
  std::unordered_map<ir::reg_id_t, size_t> first_def_global;
  std::unordered_map<ir::reg_id_t, size_t> last_use_global;
}

void number_instructions(const ir::FunctionPack& func) {
  first_def_global.clear();
  last_use_global.clear();
  size_t idx = 0;
  for (auto& bb : const_cast<ir::FunctionPack&>(func).basic_block_packs) {
    for (auto& inst : bb.instructions) {
      if (auto dst = inst->get_dst()) {
        if (!first_def_global.count(*dst)) first_def_global[*dst] = idx;
      }
      for (auto use : inst->get_uses()) {
        last_use_global[use] = idx;
      }
      ++idx;
    }
  }
}

std::vector<LiveInterval> build_intervals(const ir::FunctionPack& func) {
  number_instructions(func);

  size_t param_start = 0;
  for (const auto& param : func.params) {
    first_def_global[param.value] = param_start;
  }
  if (func.sret_param) {
    first_def_global[func.sret_param->value] = param_start;
  }

  std::unordered_set<ir::reg_id_t> all_regs;
  for (const auto& [reg, _] : first_def_global) all_regs.insert(reg);

  std::vector<LiveInterval> intervals;
  for (auto reg : all_regs) {
    size_t start = first_def_global.at(reg);
    size_t end = last_use_global.count(reg) ? last_use_global.at(reg) : start;
    intervals.push_back({reg, start, end});
  }

  std::sort(intervals.begin(), intervals.end());
  return intervals;
}


AllocationResult allocate_registers(const ir::FunctionPack& func) {
  AllocationResult result;

  auto intervals = build_intervals(func);
  std::sort(intervals.begin(), intervals.end());

  std::vector<PhysReg> reg_pool(kAllocatableRegs.begin(), kAllocatableRegs.end());

  int arg_reg_idx = 0;
  if (func.sret_param) {
    result.mapping.emplace(func.sret_param->value, Location::make_reg(PhysReg::a0));
    arg_reg_idx++;
  }
  for (const auto& param : func.params) {
    if (arg_reg_idx < 8) {
      auto pr = static_cast<PhysReg>(static_cast<uint8_t>(PhysReg::a0) + arg_reg_idx);
      result.mapping.emplace(param.value, Location::make_reg(pr));
    } else {
      result.mapping.emplace(param.value, Location::make_spill(8 * (arg_reg_idx - 8)));
    }
    ++arg_reg_idx;
  }

  std::unordered_map<PhysReg, LiveInterval> active;
  size_t spill_offset = 0;

  for (const auto& interval : intervals) {
    if (result.mapping.contains(interval.reg)) continue;

    std::erase_if(active, [&](const auto& kv) {
      return kv.second.end < interval.start;
    });

    bool allocated = false;
    for (auto pr : reg_pool) {
      if (!active.contains(pr)) {
        result.mapping.emplace(interval.reg, Location::make_reg(pr));
        active.emplace(pr, interval);
        allocated = true;
        break;
      }
    }

    if (allocated) continue;

    auto spill_it = std::max_element(active.begin(), active.end(),
      [](const auto& a, const auto& b) { return a.second.end < b.second.end; });

    if (spill_it->second.end > interval.end) {
      result.mapping[spill_it->second.reg] = Location::make_spill(spill_offset);
      spill_offset += 8;
      PhysReg freed_reg = spill_it->first;
      active.erase(spill_it);
      result.mapping.emplace(interval.reg, Location::make_reg(freed_reg));
      active.emplace(freed_reg, interval);
    } else {
      result.mapping.emplace(interval.reg, Location::make_spill(spill_offset));
      spill_offset += 8;
    }
  }

  int local_var_offset = spill_offset;
  for (const auto& bb : func.basic_block_packs) {
    for (const auto& inst : bb.instructions) {
      if (auto* alloca = dynamic_cast<const ir::AllocaInst*>(inst.get())) {
        size_t size = alloca->type.size();
        size_t align = alloca->type.align();
        local_var_offset = (local_var_offset + align - 1) & ~(align - 1);
        result.mapping[alloca->dst] = Location::make_spill(local_var_offset);
        local_var_offset += size;
      }
    }
  }

  for (auto& [vreg, loc] : result.mapping) {
    if (loc.is_reg() && kCalleeSaveRegs.contains(loc.as_reg())) {
      result.callee_saved_used.insert(loc.as_reg());
    }
  }

  size_t caller_save_area = kCallerSaveRegs.size() * 8;
  result.spill_area_size = local_var_offset;
  result.total_frame_size = result.spill_area_size
                          + result.callee_saved_used.size() * 8
                          + 8
                          + caller_save_area;
  result.total_frame_size = (result.total_frame_size + 15) & ~15ul;

  return result;
}

} // namespace rshard::backend