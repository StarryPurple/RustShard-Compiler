#ifndef RUST_SHARD_IR_PACK_H
#define RUST_SHARD_IR_PACK_H

#include <vector>

#include "syntax_check.h"
#include "ir_instruction.h"
#include "common.h"

namespace insomnia::rust_shard::ir {

// %Struct = type { type-1, ... }
struct TypeDeclarationPack {
  StringT ident;
  std::vector<IRType> field_types;

  std::string to_str() const {
    std::string res = "%" + ident + " = type { ";
    for(int i = 0; i < field_types.size(); ++i) {
      if(i > 0) res += ", ";
      res += field_types[i].to_str();
    }
    res += " }";
    return res;
  }
};

// constant values are all inlined (ignored here)

// For static string literals.
// @str = private unnamed_addr constant [N x i8] c"xxx\00", align 1
struct StaticPack {
  StringT ident;
  StringT literal;

  static std::string interpretation_string(const StringT &literal) {
    std::string str;
    for(const auto &ch: literal) {
      switch(ch) {
      case '\n': str += "\\n"; break;
      case '\t': str += "\\t"; break;
      case '\\': str += "\\\\"; break;
      case '\"': str += "\\\""; break;
      case '\'': str += "\\\'"; break;
      default: str += ch; break;
      }
    }
    return str;
  }

  std::string to_str() const {
    auto str = interpretation_string(literal);
    // use the length of the (longer) interpretation string.
    return "@" + ident + " = private unnamed_addr constant ["
    + std::to_string(str.length() + 1) + " x i8] c\"" + str + "\\00\", align 1";
  }
};

// ident:
//   lines
struct BasicBlockPack {
  Label label;
  std::vector<std::unique_ptr<Instruction>> instructions;

  std::string to_str() const {
    std::string res = label.to_str() + ":";
    for(auto &instr: instructions)
      res += "\n  " + instr->to_str();
    return res;
  }
};

struct FunctionPack {
  StringT ident;
  std::optional<std::pair<StringT, IRType>> sret_param; // if exists: stores var id str and ret_type_ref.
  IRType ret_type;
  std::vector<std::pair<StringT, IRType>> params;
  std::vector<BasicBlockPack> basic_block_packs;

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
      auto &label = basic_block_packs[i].label;
      label.label_id = i;
      umap.emplace(std::pair{label.hint, label.hint_tag_id}, i);
    }
    for(auto &basic_block: basic_block_packs) {
      for(auto &instr: basic_block.instructions) {
        if(auto b = dynamic_cast<BranchInst*>(instr.get())) {
          b->label.label_id = umap.at(std::pair{b->label.hint, b->label.hint_tag_id});
        } else if(auto c = dynamic_cast<CondBranchInst*>(instr.get())) {
          c->true_label.label_id = umap.at(std::pair{c->true_label.hint, c->true_label.hint_tag_id});
          c->false_label.label_id = umap.at(std::pair{c->false_label.hint, c->false_label.hint_tag_id});
        } else if(auto p = dynamic_cast<PhiInst*>(instr.get())) {
          for(auto &pair: p->incoming) {
            auto &label = pair.second;
            label.label_id = umap.at(std::pair{label.hint, label.hint_tag_id});
          }
        }
      }
    }
  }

  std::string to_declaration() const {
    std::string res = "declare " + (sret_param ? "void" : ret_type.to_str()) + " @" + ident + "(";
    if(sret_param) {
      res += sret_param->second.to_str() + " sret(" + ret_type.to_str() + ") %" + sret_param->first;
      if(!params.empty()) res += ", ";
    }
    for(int i = 0; i < params.size(); ++i) {
      res += params[i].second.to_str() + " %" + params[i].first;
      if(i != params.size() - 1) res += ", ";
    }
    res += ")";
    return res;
  }

  std::string to_definition() const {
    std::string res = "define " + (sret_param ? "void" : ret_type.to_str()) + " @" + ident + "(";
    if(sret_param) {
      res += sret_param->second.to_str() + " sret(" + ret_type.to_str() + ") %" + sret_param->first;
      if(!params.empty()) res += ", ";
    }
    for(int i = 0; i < params.size(); ++i) {
      res += params[i].second.to_str() + " %" + params[i].first;
      if(i != params.size() - 1) res += ", ";
    }
    res += ") {\n";
    for(auto &basic_block: basic_block_packs)
      res += basic_block.to_str() + "\n";
    res += "}";
    return res;
  }
};

struct FunctionContext {
  bool is_unreachable = false;
  int _next_reg_id = 0;
  int _next_hint_tag_id = 0;
  std::vector<BasicBlockPack> basic_block_packs;
  std::vector<std::unique_ptr<Instruction>> instructions;
  // result of node with this node id is in which register
  // break/return result is also stored.
  std::unordered_map<int, int> node_reg_map;
  // which register records the address of variable in memory, and the type of the variable
  std::vector<std::unordered_map<StringT, std::pair<int, IRType>>> variable_addr_reg_maps;
  FunctionPack function_pack;

  struct LoopContext {
    Label jump_label; // cond for while, body for loop
    Label exit_label; // exit for both while and loop
    int res_ptr_id;
  };
  std::vector<LoopContext> loop_contexts;

  // for in-place construction.
  // mapping: from node id to the given value ptr id.
  // Now only urges arrays to construct in-place.
  std::unordered_map<int, int> in_place_node_ptr_map;

  // used at PathInExpr tail sret.
  std::optional<std::pair<std::string, int>> sret_target;

  int new_reg_id() { return _next_reg_id++; }
  int new_hint_tag_id() { return _next_hint_tag_id++; }
  void init_and_start_new_block(Label &label) {
    is_unreachable = false;
    basic_block_packs.back().instructions = std::move(instructions);
    label.label_id = static_cast<int>(basic_block_packs.size()); // set label id.
    basic_block_packs.emplace_back(BasicBlockPack{.label = label});
  }
  template <class Inst> requires std::is_base_of_v<Instruction, Inst>
  void push_instruction(Inst &&inst) {
    if(is_unreachable) return;
    auto inst_ptr = std::make_unique<Inst>(std::forward<Inst>(inst));
    // auto raw_ptr = inst_ptr.get();
    instructions.emplace_back(std::move(inst_ptr));
  }
  std::pair<int, IRType> find_variable(const StringT &ident) {
    for(auto rit = variable_addr_reg_maps.rbegin(); rit != variable_addr_reg_maps.rend(); ++rit) {
      if(auto it = rit->find(ident); it != rit->end()) return it->second;
    }
    throw std::runtime_error("Variable not found");
  }
};
}


#endif // RUST_SHARD_IR_PACK_H