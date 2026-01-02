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

  void preVisit(ast::Function &node) override;
  void postVisit(ast::Function &node) override;

  void preVisit(ast::StructStruct &node) override;

  void preVisit(ast::LiteralExpression &node) override;

  void postVisit(ast::CallExpression &node) override;

  /*
  void postVisit(ast::AssignmentExpression &node) override;

  void postVisit(ast::MethodCallExpression &node) override;

  void postVisit(ast::LetStatement &node) override;
  */

private:
  static constexpr std::string kAnonyIdent = "_";
  struct TypeDeclarationPack;
  struct StaticPack;
  struct BasicBlockPack;
  struct FunctionPack;
  struct IRPack;

  std::string use_string_literal(StringT literal);

  stype::TypePool *_type_pool;
  std::unique_ptr<IRPack> _ir_pack;
  std::unordered_map<StringT, std::string> _string_literal_pool; // StringLiteral -> allocated global variable name
  std::unordered_map<StringT, int> _variable_counter;

  struct FunctionContext;
  std::vector<FunctionContext> _contexts;

};

} // namespace insomnia::rust

#endif // RUST_SHARD_IR_GENERATOR_H