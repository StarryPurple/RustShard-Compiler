#ifndef RUST_SHARD_IR_GENERATOR_H
#define RUST_SHARD_IR_GENERATOR_H

#include <filesystem>
#include "ir_pack.h"

namespace rshard::ir {
// uses some stype TypePtr. Please ensure that the type pool is still valid.
class IRGenerator: public ast::ScopedVisitor {
public:
  IRGenerator(stype::TypePool* type_pool): _type_pool(type_pool) {}
  ~IRGenerator() = default;

  IrPack release() { return std::move(_ir_pack); }

  void preVisit(ast::ConstantItem& node) override;
  void postVisit(ast::ConstantItem& node) override;

  void preVisit(ast::Function& node) override;
  void postVisit(ast::Function& node) override;

  void preVisit(ast::StructStruct& node) override;

  // pass determined types.

  void preVisit(ast::ArithmeticOrLogicalExpression& node) override;
  void preVisit(ast::ComparisonExpression& node) override;
  void preVisit(ast::CompoundAssignmentExpression& node) override;

  void preVisit(ast::LiteralExpression& node) override;
  void postVisit(ast::LiteralExpression& node) override;

  void preVisit(ast::CallExpression& node) override;
  void postVisit(ast::CallExpression& node) override;
  void preVisit(ast::MethodCallExpression& node) override;
  void postVisit(ast::MethodCallExpression& node) override;

  void preVisit(ast::LetStatement& node) override; // set addr needed
  void postVisit(ast::LetStatement& node) override;

  void postVisit(ast::AssignmentExpression& node) override;

  void preVisit(ast::BorrowExpression& node) override;
  void postVisit(ast::BorrowExpression& node) override;
  void postVisit(ast::DereferenceExpression& node) override;
  void postVisit(ast::PathInExpression& node) override;
  void preVisit(ast::IndexExpression& node) override;
  void postVisit(ast::IndexExpression& node) override;
  void postVisit(ast::TypeCastExpression& node) override;

  void postVisit(ast::NegationExpression& node) override;
  void visit(ast::ArithmeticOrLogicalExpression& node) override;
  void postVisit(ast::ComparisonExpression& node) override;
  void visit(ast::CompoundAssignmentExpression& node) override;
  void preVisit(ast::FieldExpression& node) override;
  void postVisit(ast::FieldExpression& node) override;
  void postVisit(ast::GroupedExpression& node) override;
  void visit(ast::StructExpression& node) override;
  void visit(ast::ArrayExpression& node) override;
  void preVisit(ast::BlockExpression& node) override;
  void postVisit(ast::BlockExpression& node) override;

  // branch

  void postVisit(ast::Conditions& node) override;
  void visit(ast::IfExpression& node) override;
  void visit(ast::PredicateLoopExpression& node) override;
  void visit(ast::InfiniteLoopExpression& node) override;

  void postVisit(ast::BreakExpression& node) override;
  void postVisit(ast::ContinueExpression& node) override;
  void postVisit(ast::ReturnExpression& node) override;

  void visit(ast::LazyBooleanExpression& node) override;


  // no need to look at type declarations.
  // use intended blank implementation.

  void visit(ast::ParenthesizedType& node) override {}
  void visit(ast::TupleType& node) override {}
  void visit(ast::ReferenceType& node) override {}
  void visit(ast::ArrayType& node) override {}
  void visit(ast::SliceType& node) override {}
  void visit(ast::TypePath& node) override {}

private:
  struct FunctionContext;

  bool _is_in_const = false;

  stype::TypePool* _type_pool;
  IrPack _ir_pack;
  std::unordered_map<StringT, std::string> _string_literal_pool; // StringLiteral -> allocated global variable name
  std::vector<FunctionContext> _contexts;

  std::string use_string_literal(StringT literal);

  static std::string mangle_method(stype::TypePtr impl_type, const std::string& func_name) {
    // doesn't allow to start as number.
    // return "_" + utils::to_base62(impl_type->hash()) + "<" + impl_type->to_string() + ">::" + func_name;
    return "_" + utils::to_base62(impl_type->hash()) + "_" + func_name;
  }

  IRType wrap_ref(IRType type) const {
    return type.get_ref(_type_pool);
  }

  // returns ptr_id
  reg_id_t store_into_memory(reg_id_t obj_id, IRType obj_ty);
  // returns obj_id
  reg_id_t load_from_memory(reg_id_t ptr_id, IRType obj_ty);

  struct FunctionContext {
    bool is_unreachable = false;
    reg_id_t _next_reg_id = 0;
    int _next_hint_tag_id = 0;
    std::vector<BasicBlockPack> basic_block_packs;
    std::vector<std::unique_ptr<Instruction>> instructions;
    // result of node with this node id is in which register
    // break/return result is also stored.
    std::unordered_map<int, reg_id_t> node_reg_map;
    // which register records the address of variable in memory, and the type of the variable
    std::vector<std::unordered_map<StringT, std::pair<reg_id_t, IRType>>> variable_addr_reg_maps;
    FunctionPack function_pack;

    struct LoopContext {
      Label jump_label; // cond for while, body for loop
      Label exit_label; // exit for both while and loop
      reg_id_t res_ptr_id;
    };

    std::vector<LoopContext> loop_contexts;

    // for in-place construction.
    // mapping: from node id to the given value ptr id.
    // Now only urges arrays to construct in-place.
    std::unordered_map<int, reg_id_t> in_place_node_ptr_map;

    // used at PathInExpr tail sret.
    std::optional<std::pair<std::string, reg_id_t>> sret_target;

    reg_id_t new_reg_id() { return _next_reg_id++; }
    int new_hint_tag_id() { return _next_hint_tag_id++; }

    void init_and_start_new_block(Label& label) {
      is_unreachable = false;
      basic_block_packs.back().instructions = std::move(instructions);
      label.label_id = static_cast<int>(basic_block_packs.size()); // set label id.
      basic_block_packs.emplace_back(BasicBlockPack{.label = label});
    }

    void add_reg_hint(reg_id_t id, const std::string& hint) {
      // might override old hint.
      function_pack.hints.emplace(id, hint);
    }

    /*
    void add_reg_hint_from(int id, int base_id, const std::string &append_hint) {
      auto it = function_pack.hints.find(base_id);
      std::string hint;
      if(it != function_pack.hints.end()) hint = it->second + ".";
      hint += append_hint;
      add_reg_hint(id, hint);
    }
    */
    template <class Inst> requires std::is_base_of_v<Instruction, Inst>
    void push_instruction(Inst&& inst) {
      if(is_unreachable) return;
      auto inst_ptr = std::make_unique<Inst>(std::forward<Inst>(inst));
      // auto raw_ptr = inst_ptr.get();
      instructions.emplace_back(std::move(inst_ptr));
    }

    std::pair<int, IRType> find_variable(const StringT& ident) {
      for(auto rit = variable_addr_reg_maps.rbegin(); rit != variable_addr_reg_maps.rend(); ++rit) {
        if(auto it = rit->find(ident); it != rit->end()) return it->second;
      }
      throw std::runtime_error("Variable not found");
    }
  };
};
} // namespace rshard

#endif // RUST_SHARD_IR_GENERATOR_H
