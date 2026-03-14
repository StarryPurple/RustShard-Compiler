#ifndef RUST_SHARD_IR_PACK_H
#define RUST_SHARD_IR_PACK_H

#include <vector>

#include "syntax_check.h"
#include "ir_instruction.h"
#include "common.h"

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

  // call at the final construction.
  void fill_label_ids() {
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
      label.label_id = i;
      umap.emplace(std::pair{label.hint, label.hint_tag_id}, i);
    }
    for(auto& basic_block: basic_block_packs) {
      for(auto& instr: basic_block.instructions) {
        if(auto b = dynamic_cast<BranchInst*>(instr.get())) {
          b->label.label_id = umap.at(std::pair{b->label.hint, b->label.hint_tag_id});
        } else if(auto c = dynamic_cast<CondBranchInst*>(instr.get())) {
          c->true_label.label_id = umap.at(std::pair{c->true_label.hint, c->true_label.hint_tag_id});
          c->false_label.label_id = umap.at(std::pair{c->false_label.hint, c->false_label.hint_tag_id});
        } else if(auto p = dynamic_cast<PhiInst*>(instr.get())) {
          for(auto& pair: p->incoming) {
            auto& label = pair.second;
            label.label_id = umap.at(std::pair{label.hint, label.hint_tag_id});
          }
        }
      }
    }
  }
};

struct IrPack {
  std::vector<TypeDeclarationPack> type_declaration_packs;
  std::vector<StaticPack> static_packs;
  std::vector<FunctionPack> function_packs;
};

}


#endif // RUST_SHARD_IR_PACK_H
