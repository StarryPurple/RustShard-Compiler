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
  size_t start;
  size_t end;
  bool operator<(const LiveInterval& other) const { return start < other.start; }
};

LivenessInfo compute_liveness(const ir::FunctionPack& func);
AllocationResult allocate_registers(const ir::FunctionPack& func);

} // namespace rshard::backend

#endif