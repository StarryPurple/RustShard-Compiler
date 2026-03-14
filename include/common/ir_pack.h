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
  std::unordered_map<int, std::string> hints;

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

struct IrPrinter {
  static std::string sprint(const TypeDeclarationPack& pack) {
    std::string res = "%" + pack.ident + " = type { ";
    for(int i = 0; i < pack.field_types.size(); ++i) {
      if(i > 0) res += ", ";
      res += pack.field_types[i].to_str();
    }
    res += " }";
    return res;
  }

  static std::string sprint(const StaticPack& pack) {
    auto str = interpretation_string(pack.literal);
    // use the length of the (longer) interpretation string.
    return "@" + pack.ident + " = private unnamed_addr constant ["
      + std::to_string(str.length() + 1) + " x i8] c\"" + str + "\\00\", align 1";
  }

  static std::string sprint(const BasicBlockPack& pack, const HintContext& ctx) {
    std::string res = pack.label.to_str() + ":";
    for(const auto& instr: pack.instructions)
      res += "\n  " + sprint(instr.get(), ctx);
    return res;
  }

  static std::string sprint(const FunctionPack& pack) {
    std::string res = "define " + (pack.sret_param ? "void" : pack.ret_type.to_str()) + " @" + pack.ident + "(";
    if(pack.sret_param) {
      res += pack.sret_param->second.to_str() + " sret(" + pack.ret_type.to_str() + ") %" + pack.sret_param->first;
      if(!pack.params.empty()) res += ", ";
    }
    for(int i = 0; i < pack.params.size(); ++i) {
      res += pack.params[i].second.to_str() + " %" + pack.params[i].first;
      if(i != pack.params.size() - 1) res += ", ";
    }
    res += ") {\n";
    HintContext ctx;
    ctx.hints = &pack.hints;
    for(auto& basic_block: pack.basic_block_packs)
      res += sprint(basic_block, ctx) + "\n";
    res += "}";
    return res;
  }

  static std::string sprint(const IrPack& pack) {
    std::string res;
    for(const auto& s: pack.static_packs)
      res += sprint(s) + "\n";
    res += '\n';
    for(const auto& t: pack.type_declaration_packs)
      res += sprint(t) + '\n';
    res += '\n';
    for(const auto& f: pack.function_packs)
      res += sprint(f) + "\n\n";
    return res;
  }

  static std::string sprint(const Instruction* inst, const HintContext& ctx) {
    if(!inst) throw std::runtime_error("Empty instr ptr");
    if(auto a = dynamic_cast<const AllocaInst*>(inst)) {
      return sprint(*a, ctx);
    } else if(const auto s = dynamic_cast<const StoreInst*>(inst)) {
      return sprint(*s, ctx);
    } else if(const auto l = dynamic_cast<const LoadInst*>(inst)) {
      return sprint(*l, ctx);
    } else if(const auto bi = dynamic_cast<const BinaryOpInst*>(inst)) {
      return sprint(*bi, ctx);
    } else if(const auto call = dynamic_cast<const CallInst*>(inst)) {
      return sprint(*call, ctx);
    } else if(const auto r = dynamic_cast<const ReturnInst*>(inst)) {
      return sprint(*r, ctx);
    } else if(const auto g = dynamic_cast<const GEPInst*>(inst)) {
      return sprint(*g, ctx);
    } else if(const auto cast = dynamic_cast<const CastInst*>(inst)) {
      return sprint(*cast, ctx);
    } else if(const auto br = dynamic_cast<const BranchInst*>(inst)) {
      return sprint(*br, ctx);
    } else if(const auto cond = dynamic_cast<const CondBranchInst*>(inst)) {
      return sprint(*cond, ctx);
    } else if(const auto u = dynamic_cast<const UnreachableInst*>(inst)) {
      return sprint(*u, ctx);
    } else if(const auto i = dynamic_cast<const InsertValueInst*>(inst)) {
      return sprint(*i, ctx);
    } else if(const auto p = dynamic_cast<const PhiInst*>(inst)) {
      return sprint(*p, ctx);
    } else {
      throw std::runtime_error("Unknown instruction");
    }
  }

  static std::string sprint(const AllocaInst& inst, const HintContext& ctx) {
    return ctx.hinted_reg(inst.dst) + " = alloca " + inst.type.to_str();
  }

  static std::string sprint(const StoreInst& inst, const HintContext& ctx) {
    return "store " + ctx.hinted_operand(inst.value) + ", " + ctx.hinted_operand(inst.ptr);
  }

  static std::string sprint(const LoadInst& inst, const HintContext& ctx) {
    return ctx.hinted_reg(inst.dst) + " = load " + inst.load_type.to_str() + ", "
      + ctx.hinted_operand(inst.ptr);
  }

  static std::string sprint(const BinaryOpInst& inst, const HintContext& ctx) {
    return ctx.hinted_reg(inst.dst) + " = " + inst.op + " " + inst.type.to_str()
      + " " + ctx.hinted_operand_data(inst.lhs) + ", " + ctx.hinted_operand_data(inst.rhs);
  }

  static std::string sprint(const CallInst& inst, const HintContext& ctx) {
    std::string res;
    if(inst.ret_type.to_str() != "void") {
      if(inst.dst == -1) {
        throw std::runtime_error("non-void function result not recorded");
      }
      res += ctx.hinted_reg(inst.dst) + " = ";
    }
    res += "call " + inst.ret_type.to_str() + " @" + inst.func_name + "(";
    for(int i = 0; i < inst.args.size(); ++i) {
      if(i > 0) res += ", ";
      res += ctx.hinted_operand(inst.args[i]);
    }
    res += ")";
    return res;
  }

  static std::string sprint(const ReturnInst& inst, const HintContext& ctx) {
    return "ret " + (inst.ret_val.has_value() ? ctx.hinted_operand(*inst.ret_val) : "void");
  }

  static std::string sprint(const GEPInst& inst, const HintContext& ctx) {
    std::string res = ctx.hinted_reg(inst.dst) + " = getelementptr " + inst.base_type.to_str() + ", "
      + ctx.hinted_operand(inst.ptr);
    for(auto& operand: inst.indices)
      res += ", " + ctx.hinted_operand(operand);
    return res;
  }

  static std::string sprint(const CastInst& inst, const HintContext& ctx) {
    return ctx.hinted_reg(inst.dst) + " = " + inst.cast_op() + " "
      + ctx.hinted_operand(inst.src) + " to " + inst.dst_type.to_str();
  }

  static std::string sprint(const BranchInst& inst, const HintContext& ctx) {
    return "br label %" + inst.label.to_str();
  }

  static std::string sprint(const CondBranchInst& inst, const HintContext& ctx) {
    return "br i1 " + ctx.hinted_reg(inst.cond) + ", label %" + inst.true_label.to_str()
      + ", label %" + inst.false_label.to_str();
  }

  static std::string sprint(const UnreachableInst& inst, const HintContext& ctx) {
    return "unreachable";
  }

  static std::string sprint(const InsertValueInst& inst, const HintContext& ctx) {
    std::string res;
    for(int i = 0; i < inst.operands.size(); ++i) {
      if(i > 0) res += "\n  ";
      res += ctx.hinted_reg(inst.interval_regs[i]) + " = insertvalue " + inst.type.to_str() + " ";
      res += (i > 0 ? ctx.hinted_reg(inst.interval_regs[i - 1]) : "undef");
      res += ", " + ctx.hinted_operand(inst.operands[i]);
      res += ", " + std::to_string(i);
    }
    return res;
  }

  static std::string sprint(const PhiInst& inst, const HintContext& ctx) {
    std::string res = ctx.hinted_reg(inst.dst) + " = phi " + inst.type.to_str() + " ";
    for(std::size_t i = 0; i < inst.incoming.size(); ++i) {
      if(i > 0) res += ", ";
      res += "[" + ctx.hinted_operand_data(inst.incoming[i].first) + ", %" + inst.incoming[i].second.to_str() + "]";
    }
    return res;
  }

private:
  static std::string interpretation_string(const StringT& literal) {
    std::string str;
    for(const auto& ch: literal) {
      switch(ch) {
      case '\n': str += "\\n";
        break;
      case '\t': str += "\\t";
        break;
      case '\\': str += "\\\\";
        break;
      case '\"': str += "\\\"";
        break;
      case '\'': str += "\\\'";
        break;
      default: str += ch;
        break;
      }
    }
    return str;
  }
};
}


#endif // RUST_SHARD_IR_PACK_H
