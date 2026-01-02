#ifndef RUST_SHARD_IR_GENERATOR_H
#define RUST_SHARD_IR_GENERATOR_H

#include <filesystem>

#include "syntax_check.h"
#include "IR_instruction.h"

namespace insomnia::rust_shard::ir {

// uses some stype TypePtr. Please ensure that the type pool is still valid.
class IRGenerator: public ast::ScopedVisitor {
  static constexpr std::string kAnonyIdent = "_";
  struct TypeDeclarationPack;
  struct StaticPack;
  struct BasicBlockPack;
  struct FunctionPack;
  struct IRPack;

public:
  IRGenerator(stype::TypePool *type_pool);
  ~IRGenerator();

  std::string IR_str() const;

  void preVisit(ast::Function &node) override;
  void postVisit(ast::Function &node) override;

  void preVisit(ast::StructStruct &node) override;

  void preVisit(ast::LiteralExpression &node) override;

  void postVisit(ast::CallExpression &node) override;
  void postVisit(ast::AssignmentExpression &node) override;
  /*
  void postVisit(ast::MethodCallExpression &node) override;

  void postVisit(ast::LetStatement &node) override;
  */

private:
  std::string use_string_literal(StringT literal);
  void reset_reg_id() { _next_reg_id = 0; }

  int _next_reg_id = 0;
  stype::TypePool *_type_pool;
  std::unique_ptr<IRPack> _ir_pack;
  std::unordered_map<StringT, std::string> _string_literal_pool; // StringLiteral -> allocated global variable name
  std::unordered_map<StringT, int> _variable_counter;

  std::vector<FunctionPack> _function_packs;
  std::vector<BasicBlockPack> _basic_blocks;
  std::vector<std::unique_ptr<Instruction>> _instructions;

  // result of node with this node id is in which register
  std::unordered_map<int, int> _node_reg_map;
  // which register records the address of variable in memory
  std::unordered_map<StringT, int> _variable_addr_reg_map;
};

} // namespace insomnia::rust

#endif // RUST_SHARD_IR_GENERATOR_H