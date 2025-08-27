#ifndef INSOMNIA_AST_DEFS_H
#define INSOMNIA_AST_DEFS_H

#include <unordered_map>
#include <utility>

#include "ast_fwd.h"
#include "ast_type.h"

namespace insomnia::rust_shard::ast {
enum class Operator {
  kInvalid,

  // Arithmetic Operators
  kAdd, // TokenType::kPlus
  kSub, // TokenType::kMinus
  kMul, // TokenType::kStar
  kDiv, // TokenType::kSlash
  kMod, // TokenType::kPercent
  kPow, // TokenType::kCaret

  // Compound Assignment Operators
  kAddAssign, // TokenType::kPlusEq
  kSubAssign, // TokenType::kMinusEq
  kMulAssign, // TokenType::kStarEq
  kDivAssign, // TokenType::kSlashEq
  kModAssign, // TokenType::kPercentEq
  kPowAssign, // TokenType::kCaretEq

  // Logical and Comparison Operators
  kAnd,        // TokenType::kAnd
  kOr,         // TokenType::kOr
  kNot,        // TokenType::kNot
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
  kShlAssign,        // TokenType::kShlEq
  kShrAssign,        // TokenType::kShrEq

  // Special Assignment
  kAssign, // TokenType::kEq

  // Pointer/Dereference
  kDeref, // TokenType::kStar
  kRef,   // TokenType::kAnd
};

enum class SymbolKind {
  kVariable,
  kFunction,
  kStruct,
  kEnum,
  kConstant,
};

class BasicVisitor;

class BasicNode {
public:
  virtual ~BasicNode() = default;
  virtual void accept(BasicVisitor &visitor) = 0;
};

struct SymbolInfo {
  const BasicNode* node;
  std::string_view ident;
  SymbolKind kind;
  sem_type::TypePtr type;
  bool is_const, is_mut;
};

class TypeInfo {
public:
  sem_type::TypePtr get_type() const { return _tp; } // NOLINT
  void set_type(sem_type::TypePtr tp) { _tp = std::move(tp); }
private:
  sem_type::TypePtr _tp;
};

class Scope {
public:
  void init_scope() { _symbol_set.clear(); }
  void add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    _symbol_set.emplace(ident, symbol);
  }
  SymbolInfo* find_symbol(std::string_view ident) {
    auto it = _symbol_set.find(ident);
    if(it == _symbol_set.end()) return nullptr;
    return &it->second;
  }
  const SymbolInfo* find_symbol(std::string_view ident) const {
    auto it = _symbol_set.find(ident);
    if(it == _symbol_set.end()) return nullptr;
    return &it->second;
  }
private:
  std::unordered_map<std::string_view, SymbolInfo> _symbol_set;
};

class ErrorRecorder {
public:
  void reset() { _errors.clear(); }
  void report(std::string err) { _errors.push_back(std::move(err)); }
  const std::vector<std::string>& errors() const { return _errors; }
  bool has_error() const { return !_errors.empty(); }
private:
  std::vector<std::string> _errors;
};

class ScopeInfo {
public:
  void set_scope(std::unique_ptr<Scope> scope) {
    _scope = std::move(scope);
  }
private:
  std::unique_ptr<Scope> _scope;
public:
  const std::unique_ptr<Scope>& scope() const { return _scope; }
};

}

#endif // INSOMNIA_AST_DEFS_H
