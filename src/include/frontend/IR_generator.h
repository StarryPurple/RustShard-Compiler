#ifndef RUST_SHARD_IR_GENERATOR_H
#define RUST_SHARD_IR_GENERATOR_H

#include <filesystem>

#include "syntax_check.h"
#include "IR_instruction.h"

namespace insomnia::rust_shard::ir {

// uses some stype TypePtr. Please ensure that the type pool is still valid.
class IRGenerator: public ast::ScopedVisitor {
public:
  IRGenerator(stype::TypePool *type_pool);
  ~IRGenerator();

  std::string IR_str() const;

  void preVisit(ast::ConstantItem &node) override;
  void postVisit(ast::ConstantItem &node) override;

  void preVisit(ast::Function &node) override;
  void postVisit(ast::Function &node) override;

  void preVisit(ast::StructStruct &node) override;

  // pass determined types.

  void preVisit(ast::ArithmeticOrLogicalExpression &node) override;
  void preVisit(ast::ComparisonExpression &node) override;
  void preVisit(ast::CompoundAssignmentExpression &node) override;

  void preVisit(ast::LiteralExpression &node) override;
  void postVisit(ast::LiteralExpression &node) override;

  void postVisit(ast::CallExpression &node) override;
  void postVisit(ast::MethodCallExpression &node) override;

  void preVisit(ast::LetStatement &node) override; // set addr needed
  void postVisit(ast::LetStatement &node) override;

  void postVisit(ast::AssignmentExpression &node) override;

  void postVisit(ast::BorrowExpression &node) override;
  void postVisit(ast::DereferenceExpression &node) override;
  void postVisit(ast::PathInExpression &node) override;
  void preVisit(ast::IndexExpression &node) override;
  void postVisit(ast::IndexExpression &node) override;
  void postVisit(ast::TypeCastExpression &node) override;

  void postVisit(ast::NegationExpression &node) override;
  void postVisit(ast::ArithmeticOrLogicalExpression &node) override;
  void postVisit(ast::ComparisonExpression &node) override;
  void postVisit(ast::CompoundAssignmentExpression &node) override;
  void preVisit(ast::FieldExpression &node) override;
  void postVisit(ast::FieldExpression &node) override;
  void postVisit(ast::GroupedExpression &node) override;
  void visit(ast::StructExpression &node) override;
  void visit(ast::ArrayExpression &node) override;
  void postVisit(ast::BlockExpression &node) override;

  // branch

  void postVisit(ast::Conditions &node) override;
  void visit(ast::IfExpression &node) override;
  void visit(ast::PredicateLoopExpression &node) override;
  void visit(ast::InfiniteLoopExpression &node) override;

  void postVisit(ast::BreakExpression &node) override;
  void postVisit(ast::ContinueExpression &node) override;
  void postVisit(ast::ReturnExpression &node) override;

  void visit(ast::LazyBooleanExpression &node) override;


  // no need to look at type declarations.
  // use intended blank implementation.

  void visit(ast::ParenthesizedType &node) override {}
  void visit(ast::TupleType &node) override {}
  void visit(ast::ReferenceType &node) override {}
  void visit(ast::ArrayType &node) override {}
  void visit(ast::SliceType &node) override {}
  void visit(ast::TypePath &node) override {}

private:
  struct TypeDeclarationPack;
  struct StaticPack;
  struct BasicBlockPack;
  struct FunctionPack;
  struct IRPack;


  bool _is_in_const = false;

  stype::TypePool *_type_pool;
  std::unique_ptr<IRPack> _ir_pack;
  std::unordered_map<StringT, std::string> _string_literal_pool; // StringLiteral -> allocated global variable name

  struct FunctionContext;
  std::vector<FunctionContext> _contexts;

  std::string use_string_literal(StringT literal);
  static std::string mangle_method(stype::TypePtr impl_type, const std::string &func_name) {
    // doesn't allow to start as number.
    return "_" + utils::to_base62(impl_type->hash()) + "_" + func_name;
  }
  // returns ptr_id
  int store_into_memory(int obj_id, IRType obj_ty);
  // returns obj_id
  int load_from_memory(int ptr_id, IRType obj_ty);

};

} // namespace insomnia::rust

#endif // RUST_SHARD_IR_GENERATOR_H