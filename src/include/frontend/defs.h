#ifndef RUST_SHARD_FRONTEND_AST_DEFS_H
#define RUST_SHARD_FRONTEND_AST_DEFS_H

#include <unordered_map>
#include <utility>

#include "common.h"
#include "constant.h"
#include "fwd.h"
#include "type.h"

namespace insomnia::rust_shard::ast {

enum class SymbolKind {
  kVariable,
  kFunction,
  kStruct,
  kEnum,
  kConstant, // Variant
  kTrait,
  kTypeAlias,
  kPrimitiveType
};

class BasicVisitor;
class RecursiveVisitor;

class BasicNode {
public:
  BasicNode(): _id(_next_node_id++) {}
  virtual ~BasicNode() = default;
  virtual void accept(BasicVisitor &visitor) = 0;
  int id() const { return _id; }
private:
  const int _id;

public:
  static void reset_id_counter() { _next_node_id = 0; }
private:
  static int _next_node_id;
};
inline int BasicNode::_next_node_id = 0;

class ASTTree {
public:
  explicit ASTTree(std::unique_ptr<Crate> crate);
  void traverse(RecursiveVisitor &r_visitor);
private:
  std::unique_ptr<Crate> _crate;
};

struct SymbolInfo {
  const BasicNode* node;
  StringT ident;
  SymbolKind kind;
  stype::TypePtr type;
  bool is_place_mut; // variable
  sconst::ConstValPtr cval; // const
};

class TypeInfo {
public:
  stype::TypePtr get_type() const { return _tp; } // NOLINT
  virtual bool set_type(stype::TypePtr tp) {
    /* might need reset, like kInt -> kI32 or kFloat -> kF32
    if(_tp.operator bool())
      throw std::runtime_error("TypeInfo already set");
    */
    _tp = std::move(tp);
    return true;
  }
private:
  stype::TypePtr _tp;
};

class Scope {
public:
  Scope(int id): _id(id) {}
  int id() const { return _id; }

  SymbolInfo* add_symbol(const StringT &ident, const SymbolInfo &symbol);
  SymbolInfo* find_symbol(const StringT &ident);
  const SymbolInfo* find_symbol(const StringT &ident) const;
  bool set_type(const StringT &ident, stype::TypePtr type);
  const auto& symbol_set() const { return _symbol_set; }

private:
  std::unordered_map<StringT, SymbolInfo> _symbol_set;
  int _id;
};

class ScopeInfo {
public:
  void set_scope(std::unique_ptr<Scope> scope) {
    if(_scope.operator bool())
      throw std::runtime_error("Scope already set");
    _scope = std::move(scope);
  }
  const std::unique_ptr<Scope>& scope() const { return _scope; } // NOLINT
private:
  std::unique_ptr<Scope> _scope;
};

class ErrorRecorder {
public:
  void reset() { _tagged_errors.clear(); _untagged_errors.clear(); }
  // only the earliest error with the tag will be stored
  void tagged_report(std::string tag, std::string err) {
    if(!_tagged_errors.contains(tag)) {
      _tagged_errors.emplace(std::move(tag), std::move(err));
    }
  }
  void report(std::string err) {
    _untagged_errors.push_back(std::move(err));
  }
  bool has_error() const { return !_untagged_errors.empty() || !_tagged_errors.empty(); }
private:
  std::unordered_map<std::string, std::string> _tagged_errors;
  std::vector<std::string> _untagged_errors;
public:
  const decltype(_tagged_errors)& tagged_errors() const { return _tagged_errors; }
  const decltype(_untagged_errors)& untagged_errors() const { return _untagged_errors; }
};


class ResolutionInfo {

};

class ResolutionTree {

};

// every crate owns one.
struct Module {
  /*
  std::unordered_map<StringRef, stype::AliasType> aliases;
  std::unordered_map<StringRef, stype::FunctionType> functions;
  std::unordered_map<StringRef, stype::TraitType> traits;
  */
  std::unordered_map<
    stype::TypePtr,
    std::unordered_map<StringT, std::shared_ptr<stype::FunctionType>>,
    stype::TypePtr::Hash,
    stype::TypePtr::Equal> _methods;

  Module() = default;
};


enum class Operator {
  kInvalid,

  // Arithmetic Operators
  kAdd, // TokenType::kPlus
  kSub, // TokenType::kMinus
  kMul, // TokenType::kStar
  kDiv, // TokenType::kSlash
  kMod, // TokenType::kPercent

  // Compound Assignment Operators
  kAddAssign, // TokenType::kPlusEq
  kSubAssign, // TokenType::kMinusEq
  kMulAssign, // TokenType::kStarEq
  kDivAssign, // TokenType::kSlashEq
  kModAssign, // TokenType::kPercentEq

  // Logical and Comparison Operators
  kLogicalNot,        // TokenType::kNot
  kLogicalAnd, // TokenType::kAndAnd
  kLogicalOr,  // TokenType::kOrOr
  kEq,         // TokenType::kEqEq
  kNe,         // TokenType::kNe
  kGt,         // TokenType::kGt
  kLt,         // TokenType::kLt
  kGe,         // TokenType::kGe
  kLe,         // TokenType::kLe

  // Bitwise Operators
  kBitwiseAnd,       // TokenType::kAnd
  kBitwiseOr,        // TokenType::kOr
  kBitwiseXor,       // TokenType::kCaret
  kShl,              // TokenType::kShl
  kShr,              // TokenType::kShr
  kBitwiseAndAssign, // TokenType::kAndEq
  kBitwiseOrAssign,  // TokenType::kOrEq
  kBitwiseXorAssign, // TokenType::kCaretEq
  kShlAssign,        // TokenType::kShlEq
  kShrAssign,        // TokenType::kShrEq

  // Special Assignment
  kAssign, // TokenType::kEq

  // Pointer/Dereference
  kDeref, // TokenType::kStar
  kRef,   // TokenType::kAnd
};

}

#endif // RUST_SHARD_FRONTEND_AST_DEFS_H
