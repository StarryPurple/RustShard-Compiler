#ifndef INSOMNIA_AST_DEFS_H
#define INSOMNIA_AST_DEFS_H

#include <unordered_map>
#include <utility>

#include "ast_constant.h"
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
  std::string _crate_name = "__single_crate__";
};

struct SymbolInfo {
  const BasicNode* node;
  std::string_view ident;
  SymbolKind kind;
  bool is_mut;
  sem_type::TypePtr type;
};

class TypeInfoBase {
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

class Scope {
public:
  Scope() = default;
  void load_builtin_types(sem_type::TypePool *pool);
  SymbolInfo* add_symbol(std::string_view ident, const SymbolInfo &symbol);
  SymbolInfo* find_symbol(std::string_view ident);
  const SymbolInfo* find_symbol(std::string_view ident) const;
  bool set_type(std::string_view ident, sem_type::TypePtr type);

private:
  std::unordered_map<std::string_view, SymbolInfo> _symbol_set;
};

// related AST node:
// Crate, Trait, Implementation (InherentImpl + TraitImpl),
// BlockExpression, FunctionBodyExpr, MatchArm
class ScopeInfoBase {
public:
  void set_scope(std::unique_ptr<Scope> scope) {
    if(_scope)
      throw std::runtime_error("Scope already set");
    _scope = std::move(scope);
  }
  const std::unique_ptr<Scope>& scope() const { return _scope; }
private:
  std::unique_ptr<Scope> _scope;
};

struct ResolutionPath {
  std::vector<std::string_view> segments;
  bool is_absolute;
};

enum class ResolutionKind {
  kProject,        // no related semantic object
  kCrate,          // no related semantic object
  kTrait,          // trait_type
  kInherentImpl,   // obj_type
  kTraitImpl,      // obj_type, trait_type
  kType,           // obj_type
  kFunction,       // obj_type
  kConstant,       // const_val
};

// stands for an associative type/function/constant/trait
class ResolutionNode {
  std::string_view _ident;
  ResolutionKind _kind;
  sem_type::TypePtr _obj_type;
  sem_type::TypePtr _trait_type;
  sem_const::ConstValPtr _const_val;
  ResolutionNode *_super, *_crate, *_Self;
  std::unordered_map<std::string_view, std::unique_ptr<ResolutionNode>> _children;

public:
  ResolutionNode() = default;

  // Project -> [Crate+]
  static std::unique_ptr<ResolutionNode> make_project(std::string_view ident) {
    auto r = std::make_unique<ResolutionNode>();
    r->_ident = ident;
    r->_kind = ResolutionKind::kProject;
    r->_super = nullptr;
    r->_crate = nullptr;
    r->_Self = nullptr;
    return r;
  }

  // Crate -> [VisItem*]
  std::unique_ptr<ResolutionNode> make_crate(std::string_view ident) {
    auto r = std::make_unique<ResolutionNode>();
    r->_ident = ident;
    r->_kind = ResolutionKind::kCrate;
    r->_super = this;
    r->_crate = r.get();
    r->_Self = nullptr;
    return r;
  }

  // trait Trait
  std::unique_ptr<ResolutionNode> make_trait(sem_type::TypePtr trait_type) {
    auto r = std::make_unique<ResolutionNode>();
    r->_kind = ResolutionKind::kTrait;
    r->_trait_type = trait_type;
    r->_super = this;
    r->_crate = _crate;
    r->_Self = nullptr;
    return r;
  }

  // impl Type
  std::unique_ptr<ResolutionNode> make_inherent_impl(sem_type::TypePtr obj_type) {
    auto r = std::make_unique<ResolutionNode>();
    r->_kind = ResolutionKind::kInherentImpl;
    r->_obj_type = obj_type;
    r->_super = this;
    r->_crate = _crate;
    r->_Self = ;
    return r;
  }

  std::unique_ptr<ResolutionNode> make_trait_impl(sem_type::TypePtr obj_type, sem_type::TypePtr trait_type) {
    auto r = std::make_unique<ResolutionNode>();
    r->_kind = ResolutionKind::kTraitImpl;
    return r;
  }

  std::unique_ptr<ResolutionNode> make_type_alias(sem_type::TypePtr obj_type) {
    auto r = std::make_unique<ResolutionNode>();
    r->_kind = ResolutionKind::kInherentImpl;
    return r;
  }

  std::unique_ptr<ResolutionNode> make_function(sem_type::TypePtr obj_type) {
    auto r = std::make_unique<ResolutionNode>();
    return r;
  }

  std::unique_ptr<ResolutionNode> make_constant(sem_const::ConstValPtr const_val) {
    auto r = std::make_unique<ResolutionNode>();
    return r;
  }

  ResolutionNode* resolve_segment(std::string_view ident) const {
    if(auto it = _children.find(ident); it != _children.end()) {
      return it->second.get();
    }
    return nullptr;
  }
};

class ResolutionTree {
public:
  ResolutionTree(std::string_view project_ident): _root(ResolutionNode::make_project(project_ident)) {}
  ResolutionNode *resolve_path(const ResolutionPath &path, ResolutionNode *current) const;
private:
  std::unique_ptr<ResolutionNode> _root; // project layer
};

}

#endif // INSOMNIA_AST_DEFS_H
