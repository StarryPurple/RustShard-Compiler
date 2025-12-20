#ifndef INSOMNIA_AST_DEFS_H
#define INSOMNIA_AST_DEFS_H

#include <unordered_map>
#include <utility>

#include "common.h"
#include "ast_fwd.h"
#include "ast_type.h"

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
  virtual ~BasicNode() = default;
  virtual void accept(BasicVisitor &visitor) = 0;
};

class ASTTree {
public:
  explicit ASTTree(std::unique_ptr<Crate> crate);
  void traverse(RecursiveVisitor &r_visitor);
private:
  std::unique_ptr<Crate> _crate;
};

struct SymbolInfo {
  const BasicNode* node;
  StringRef ident;
  SymbolKind kind;
  stype::TypePtr type;
  bool is_place_mut;
};

class TypeInfo {
public:
  stype::TypePtr get_type() const { return _tp; } // NOLINT
  void set_type(stype::TypePtr tp) {
    if(_tp.operator bool())
      throw std::runtime_error("TypeInfo already set");
    _tp = std::move(tp);
  }
private:
  stype::TypePtr _tp;
};

class Scope {
public:
  void init_scope() { _symbol_set.clear(); }
  void load_builtin(stype::TypePool *pool);

  SymbolInfo* add_symbol(const StringRef &ident, const SymbolInfo &symbol);
  SymbolInfo* find_symbol(const StringRef &ident);
  const SymbolInfo* find_symbol(const StringRef &ident) const;
  bool set_type(const StringRef &ident, stype::TypePtr type);

private:
  std::unordered_map<StringRef, SymbolInfo> _symbol_set;
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
  StringRef ident; // module name
  std::unordered_map<StringRef, stype::AliasType> aliases;
  std::unordered_map<StringRef, stype::FunctionType> functions;
  std::unordered_map<StringRef, stype::TraitType> traits;

  explicit Module(StringRef _ident): ident(std::move(_ident)) {}
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

#endif // INSOMNIA_AST_DEFS_H
