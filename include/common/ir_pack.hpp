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
  std::vector<IrType> field_types;
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

struct DomTree {
  // immediate dominator. -1 for invalid.
  std::vector<block_id_t> idom;
  // children[i] = {j: idom[j] = i}
  std::vector<std::vector<block_id_t>> children;
  // domination frontier.
  std::vector<std::vector<block_id_t>> df;
  bool valid = false;
};

struct CFGData {
  // tree edges.
  std::vector<std::vector<block_id_t>> pred;
  std::vector<std::vector<block_id_t>> succ;
  bool valid = false;

  void add_edge(block_id_t from, block_id_t to) {
    pred[to].push_back(from);
    succ[from].push_back(to);
  }
};

struct FunctionPack {
  StringT ident;
  std::optional<Operand> sret_param; // if exists: stores var id str and ret_type_ref.
  IrType ret_type;
  std::vector<Operand> params;
  std::vector<BasicBlockPack> basic_block_packs;
  std::unordered_map<reg_id_t, std::string> hints;

  CFGData cfg;
  DomTree dom_tree;

  // call at the final construction.
  void update_block_ids();

  void construct_cfg();
  void construct_domtree();

  void reorder_reg_ids();
};

struct IrPack {
  std::vector<TypeDeclarationPack> type_declaration_packs;
  std::vector<StaticPack> static_packs;
  std::vector<FunctionPack> function_packs;
};

}


#endif // RUST_SHARD_IR_PACK_H
