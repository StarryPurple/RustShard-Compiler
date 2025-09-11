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
  virtual ~BasicNode() = default;
  virtual void accept(BasicVisitor &visitor) = 0;
};

class ASTTree {
public:
  ASTTree(std::unique_ptr<Crate> crate);
  void traverse(RecursiveVisitor &r_visitor);
private:
  std::unique_ptr<Crate> _crate;
};

struct SymbolInfo {
  const BasicNode* node;
  std::string_view ident;
  SymbolKind kind;
  bool is_mut;
  sem_type::TypePtr type;
};

class TypeInfo {
public:
  sem_type::TypePtr get_type() const { return _tp; } // NOLINT
  void set_type(sem_type::TypePtr tp) {
    if(_tp.operator bool())
      throw std::runtime_error("TypeInfo already set");
    _tp = std::move(tp);
  }
private:
  sem_type::TypePtr _tp;
};

class Scope {
public:
  void init_scope() { _symbol_set.clear(); }
  void load_builtin_types(sem_type::TypePool *pool) {
    for(const auto prime: sem_type::type_primes()) {
      auto ident = sem_type::get_type_view_from_prime(prime);
      _symbol_set.emplace(ident, SymbolInfo{
        .node = nullptr,
        .ident = ident,
        .kind = SymbolKind::kPrimitiveType,
        .type = pool->make_type<sem_type::PrimitiveType>(prime)
      });
    }
  }
  SymbolInfo* add_symbol(std::string_view ident, const SymbolInfo &symbol);
  SymbolInfo* find_symbol(std::string_view ident);
  const SymbolInfo* find_symbol(std::string_view ident) const;
  bool set_type(std::string_view ident, sem_type::TypePtr type);

private:
  std::unordered_map<std::string_view, SymbolInfo> _symbol_set;
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

class ScopeInfo {
public:
  void set_scope(std::unique_ptr<Scope> scope) {
    if(_scope.operator bool())
      throw std::runtime_error("Scope already set");
    _scope = std::move(scope);
  }
  const std::unique_ptr<Scope>& scope() const { return _scope; }
private:
  std::unique_ptr<Scope> _scope;
};

/*
class ControlBlockContext {
public:
  void add_loop_break_asso(LoopExpression *loop_expr, BreakExpression *break_expr) {
    auto it = _loop_breaks.find(loop_expr);
    if(it == _loop_breaks.end()) {
      _loop_breaks.emplace(loop_expr, std::vector{break_expr});
    } else {
      it->second.push_back(break_expr);
    }
  }
  void add_func_return_asso(FunctionBodyExpr *func_expr, ReturnExpression *return_expr) {
    auto it = _func_returns.find(func_expr);
    if(it == _func_returns.end()) {
      _func_returns.emplace(func_expr, std::vector{return_expr});
    } else {
      it->second.push_back(return_expr);
    }
  }
private:
  std::unordered_map<LoopExpression*, std::vector<BreakExpression*>> _loop_breaks;
  std::unordered_map<FunctionBodyExpr*, std::vector<ReturnExpression*>> _func_returns;
};
*/

}

#endif // INSOMNIA_AST_DEFS_H
