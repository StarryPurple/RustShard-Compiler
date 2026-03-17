#ifndef RUST_SHARD_IR_PACK_H
#define RUST_SHARD_IR_PACK_H

#include <vector>
#include <cstring>

#include "frontend/syntax_check.hpp"
#include "common/ir_instruction.hpp"
#include "common/common.hpp"

namespace rshard::ir {
// %Struct = type { type-1, ... }
struct TypeDeclarationPack {
  StringT ident;
  std::vector<IRType> field_types;
};

// constant values are all inlined (ignored here)
// struct ConstantPack;

// For static string literals.
// @str = private unnamed_addr constant [N x i8] c"xxx\00", align 1
struct StaticPack {
  StringT ident;
  StringT literal;
};

// ident:
//   lines
struct BasicBlockPack {
  Label label;
  std::vector<std::unique_ptr<Instruction>> instructions;
};

struct FunctionPack {
  StringT ident;
  std::optional<std::pair<StringT, IRType>> sret_param; // if exists: stores var id str and ret_type_ref.
  IRType ret_type;
  std::vector<std::pair<StringT, IRType>> params;
  std::vector<BasicBlockPack> basic_block_packs;
  std::unordered_map<reg_id_t, std::string> hints;

  struct CFGData {
    std::vector<std::vector<block_id_t>> pred, succ;
    std::vector<block_id_t> idom; // immediate dominator. -1 for invalid.
    std::vector<std::vector<block_id_t>> children; // domtree children
    bool valid = false;
  };
  CFGData cfg;

  // call at the final construction.
  void update_block_ids() {
    struct PairHash {
      std::size_t operator()(const std::pair<LabelHint, int>& pair) const {
        std::size_t h1 = std::hash<int>{}(static_cast<int>(pair.first));
        std::size_t h2 = std::hash<int>{}(pair.second);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
      }
    };
    std::unordered_map<std::pair<LabelHint, int>, int, PairHash> umap;
    for(int i = 0; i < basic_block_packs.size(); ++i) {
      auto& label = basic_block_packs[i].label;
      label.block_id = i;
      umap.emplace(std::pair{label.hint, label.hint_id}, i);
    }
    for(auto& basic_block: basic_block_packs) {
      for(auto& instr: basic_block.instructions) {
        if(auto b = dynamic_cast<BranchInst*>(instr.get())) {
          b->label.block_id = umap.at(std::pair{b->label.hint, b->label.hint_id});
        } else if(auto c = dynamic_cast<CondBranchInst*>(instr.get())) {
          c->true_label.block_id = umap.at(std::pair{c->true_label.hint, c->true_label.hint_id});
          c->false_label.block_id = umap.at(std::pair{c->false_label.hint, c->false_label.hint_id});
        } else if(auto p = dynamic_cast<PhiInst*>(instr.get())) {
          for(auto& pair: p->incoming) {
            auto& label = pair.second;
            label.block_id = umap.at(std::pair{label.hint, label.hint_id});
          }
        }
      }
    }
  }

  void invalidate_cfg() {
    cfg.valid = false;
  }
  void construct_cfg() {
    if(cfg.valid) return;

    struct Bitmap {
      std::vector<std::uint64_t> map;
      int width;

      explicit Bitmap(int n, bool val): map((n + 63) / 64), width(n) {
        if(val) {
          std::memset(map.data(), 0xff, map.size() * sizeof(std::uint64_t));
          map.back() &= (1ull << (n % 64)) - 1;
        }
      }
      bool get(int p) const {
        if(p < 0 || p >= width) {
          throw std::runtime_error("Unexpected usage");
        }
        return (map[p / 64] >> (p % 64)) & 1;
      }
      bool set(int p, bool val) {
        if(p < 0 || p >= width) {
          throw std::runtime_error("Unexpected usage");
        }
        if(val) {
          map[p / 64] |= (1ull << (p % 64));
        } else {
          map[p / 64] &= ~(1ull << (p % 64));
        }
      }
      Bitmap operator|(const Bitmap &other) const {
        if(width != other.width) {
          throw std::runtime_error("Unexpected usage");
        }
        Bitmap res(width, false);
        for(int i = 0; i < map.size(); ++i)
          res.map[i] = map[i] | other.map[i];
        return res;
      }
      Bitmap operator&(const Bitmap &other) const {
        if(width != other.width) {
          throw std::runtime_error("Unexpected usage");
        }
        Bitmap res(width, false);
        for(int i = 0; i < map.size(); ++i)
          res.map[i] = map[i] & other.map[i];
        return res;
      }
      Bitmap& operator|=(const Bitmap &other) {
        if(width != other.width) {
          throw std::runtime_error("Unexpected usage");
        }
        for(int i = 0; i < map.size(); ++i)
          map[i] |= other.map[i];
        return *this;
      }
      Bitmap& operator&=(const Bitmap &other) {
        if(width != other.width) {
          throw std::runtime_error("Unexpected usage");
        }
        for(int i = 0; i < map.size(); ++i)
          map[i] &= other.map[i];
        return *this;
      }
      bool operator==(const Bitmap &other) const {
        return map == other.map;
      }
      bool operator!=(const Bitmap &other) const {
        return map != other.map;
      }
    };

    block_id_t num_block = basic_block_packs.size();
    cfg.pred.clear();
    cfg.succ.clear();
    cfg.pred.resize(num_block, {});
    cfg.succ.resize(num_block, {});

    auto add_edge = [&](block_id_t from, block_id_t to) {
      cfg.pred[to].push_back(from);
      cfg.succ[from].push_back(to);
    };

    for(int id = 0; id < num_block; ++id) {
      Instruction* inst = basic_block_packs[id].instructions.back().get();
      // termination: ReturnInst / BranchInst / CondBranchInst / UnreachableInst
      if(auto r = dynamic_cast<ReturnInst*>(inst)) {
        // nothing here.
      } else if(auto b = dynamic_cast<BranchInst*>(inst)) {
        add_edge(id, b->label.block_id);
      } else if(auto c = dynamic_cast<CondBranchInst*>(inst)) {
        add_edge(id, c->true_label.block_id);
        add_edge(id, c->false_label.block_id);
      } else if(auto u = dynamic_cast<UnreachableInst*>(inst)) {
        // nothing here.
      } else {
        throw std::runtime_error("Unrecognized termination instruction");
      }
    }

    std::vector<Bitmap> doms;
    doms.resize(num_block, Bitmap{num_block, true});
    doms[0] = Bitmap{num_block, false};
    doms[0].set(0, true);

    bool changed = true;
    while(changed) {
      changed = false;
      for(int i = 1; i < num_block; ++i) {
        Bitmap tmp{num_block, true};
        for(const auto &p: cfg.pred[i]) {
          tmp &= doms[p];
        }
        tmp.set(i, true);
        if(doms[i] != tmp) {
          doms[i] = tmp;
          changed = true;
        }
      }
    }

    cfg.idom.clear();
    cfg.children.clear();
    cfg.idom.resize(num_block, -1);
    cfg.children.resize(num_block, {});

    for(int i = 1; i < num_block; ++i) {
      int new_idom = -1;
      for(int j = 0; j < num_block; ++j) {
        if(j == i || !doms[i].get(j)) continue;
        if(new_idom == -1 || doms[j].get(new_idom)) {
          new_idom = j;
        }
      }
      cfg.idom[i] = new_idom;
      cfg.children[new_idom].push_back(i);
    }

    cfg.valid = true;
  }
};

struct IrPack {
  std::vector<TypeDeclarationPack> type_declaration_packs;
  std::vector<StaticPack> static_packs;
  std::vector<FunctionPack> function_packs;
};

}


#endif // RUST_SHARD_IR_PACK_H
