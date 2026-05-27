#ifndef RUST_SHARD_REGALLOC_HPP
#define RUST_SHARD_REGALLOC_HPP

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include "ir/ir_pack.hpp"
#include "backend/asm_pack.hpp"

namespace rshard::backend {
struct LivenessInfo {
  std::unordered_map<ir::block_id_t, std::unordered_set<ir::reg_id_t>> live_in;
  std::unordered_map<ir::block_id_t, std::unordered_set<ir::reg_id_t>> live_out;
  std::unordered_map<ir::block_id_t, std::unordered_set<ir::reg_id_t>> def;
  std::unordered_map<ir::block_id_t, std::unordered_set<ir::reg_id_t>> use;
};

struct LiveInterval {
  ir::reg_id_t reg;
  ir::instr_no_t start;
  ir::instr_no_t end;
  bool operator<(const LiveInterval& other) const { return start < other.start; }
};

// Stack:
//                             <- $sp of the caller
//   - func args on stack (8+) (set by caller)        // Access at entry
//   - (possible padding for $sp 16-byte alignment)
//   - local variable allocate (AllocaInst addresses) // No direct access. It can be LARGE.
//   - return addr ($ra)                              // Access at entry and leave
//   - callee save registers                          // Access at entry and leave
//   - caller save registers                          // Access at each function call
//   - virtual register spill (spill slots)           // Access once each.
//                             <- (current) $sp of the callee (16-byte aligned)
struct AllocationResult {
  std::unordered_map<ir::reg_id_t, Location> mapping;
  std::unordered_set<PhysReg> caller_saved_used;
  std::unordered_set<PhysReg> callee_saved_used; // union of @caller_to_save
  std::unordered_map<PhysReg, std::vector<LiveInterval>> preg_interval;
  std::unordered_map<const ir::CallInst*, std::unordered_set<PhysReg>> caller_to_save;
  std::size_t total_frame_size;
  std::size_t spill_area_size;
  std::size_t local_var_size;
  std::size_t max_caller_save_num;
  std::size_t spill_args_num;

  std::size_t ra_area_needed() const { return caller_to_save.empty() ? 0 : 8; }

  offset_t spill_regs_offset() const { return 0; }
  offset_t caller_save_offset() const { return spill_area_size; }
  offset_t callee_save_offset() const { return caller_save_offset() + max_caller_save_num * 8; }
  offset_t return_addr_offset() const { return callee_save_offset() + callee_saved_used.size() * 8; }
  offset_t local_var_offset() const { return return_addr_offset() + ra_area_needed();  }
  offset_t spill_args_offset() const { return total_frame_size - spill_args_num * 8; }
};

LivenessInfo compute_liveness(const ir::FunctionPack& func);
AllocationResult allocate_registers(const ir::FunctionPack& func);
} // namespace rshard::backend

#endif
