#ifndef INSOMNIA_SYNTAX_CHECK_H
#define INSOMNIA_SYNTAX_CHECK_H

#include <unordered_map>

#include "ast_recursive_visitor.h"
#include "ast_type.h"
#include "parser.h"

namespace insomnia::rust_shard::ast {

// check branch syntax (break/continue/return), set scopes and collect symbols
class SymbolCollector : public RecursiveVisitor {
public:
  explicit SymbolCollector(ErrorRecorder *recorder): _recorder(recorder) {}

  // scope related:
  // Crate, BlockExpression, MatchArms, Function, Impl, Trait

  void preVisit(Crate &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Crate &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(BlockExpression &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(BlockExpression &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void visit(MatchArms &node) override {
    // no preVisit
    for(const auto &[arm, expr]: node.arms()) {
      _scopes.push_back(std::make_unique<Scope>());
      arm->accept(*this);
      expr->accept(*this);
      arm->set_scope(std::move(_scopes.back()));
      _scopes.pop_back();
    }
    // no postVisit
  }
  void preVisit(Function &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction
    }); // add to outer scope
    if(!flag)
      _recorder->report("Function symbol already defined: " + std::string(node.ident()));
    _scopes.push_back(std::make_unique<Scope>());
    _function_cnt++;
  }
  void postVisit(Function &node) override {
    if(node.body_opt())
      node.body_opt()->set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
    _function_cnt--;
  }
  void preVisit(InherentImpl &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(InherentImpl &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(TraitImpl &node) override {
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(TraitImpl &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }
  void preVisit(Trait &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTrait
    });
    if(!flag)
      _recorder->report("Trait symbol already defined: " + std::string(node.ident()));
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Trait &node) override {
    node.set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
  }

  // symbol related:
  // Function(dealt), const, struct, trait(dealt), enum, type alias
  // patterns (including MatchArm pattern and Let pattern)
  void preVisit(ConstantItem &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kConstant
    });
    if(!flag)
      _recorder->report("ConstantItem symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(StructStruct &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kStruct
    });
    if(!flag)
      _recorder->report("StructStruct symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(Enumeration &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kEnum
    });
    if(!flag)
      _recorder->report("Enumeration symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(IdentifierPattern &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kVariable
    });
    if(!flag)
      _recorder->report("Variable symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(TypeAlias &node) override {
    auto flag = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTypeAlias
    });
    if(!flag)
      _recorder->report("TypaAlias typename already used: " + std::string(node.ident()));
  }

  // type related:
  // struct(dealt), enum(dealt), type alias(dealt)

  // loop check: (continue, break)
  void preVisit(InfiniteLoopExpression &node) override {
    _loop_cnt++;
  }
  void postVisit(InfiniteLoopExpression &node) override {
    _loop_cnt--;
  }
  void preVisit(PredicateLoopExpression &node) override {
    _loop_cnt++;
  }
  void postVisit(PredicateLoopExpression &node) override {
    _loop_cnt--;
  }
  void preVisit(BreakExpression &node) override {
    if(_loop_cnt == 0)
      _recorder->report("break expression not inside a loop.");
  }
  void preVisit(ContinueExpression &node) override {
    if(_loop_cnt == 0)
      _recorder->report("continue expression not inside a loop.");
  }

  // function check: (return)
  void preVisit(ReturnExpression &node) override {
    if(_function_cnt == 0)
      _recorder->report("return expression not inside a function.");
  }

private:
  ErrorRecorder *_recorder;
  std::vector<std::unique_ptr<Scope>> _scopes; // store the constructing scopes
  int _loop_cnt = 0, _function_cnt = 0;

  bool add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
};

// Affected:
//
// preVisit, postVisit: Crate, BlockExpression, Function, InherentImpl, TraitImpl, Trait
//
// visit: MatchArms,
class ScopedVisitor : public RecursiveVisitor {
public:
  ScopedVisitor() = default;

  void preVisit(Crate &node) override {
    _scopes.push_back(node.scope().get());
  }
  void postVisit(Crate &node) override {
    _scopes.pop_back();
  }
  void preVisit(BlockExpression &node) override {
    _scopes.push_back(node.scope().get());
  }
  void postVisit(BlockExpression &node) override {
    _scopes.pop_back();
  }
  void visit(MatchArms &node) override {
    // no preVisit
    for(const auto &[arm, expr]: node.arms()) {
      _scopes.push_back(arm->scope().get());
      arm->accept(*this);
      expr->accept(*this);
      _scopes.pop_back();
    }
    // no postVisit
  }
  void preVisit(Function &node) override {
    if(node.body_opt())
      _scopes.push_back(node.body_opt()->scope().get());
  }
  void postVisit(Function &node) override {
    if(node.body_opt())
      _scopes.pop_back();
  }
  void preVisit(InherentImpl &node) override {
    _scopes.push_back(node.scope().get());
  }
  void postVisit(InherentImpl &node) override {
    _scopes.pop_back();
  }
  void preVisit(TraitImpl &node) override {
    _scopes.push_back(node.scope().get());
  }
  void postVisit(TraitImpl &node) override {
    _scopes.pop_back();
  }
  void preVisit(Trait &node) override {
    _scopes.push_back(node.scope().get());
  }
  void postVisit(Trait &node) override {
    _scopes.pop_back();
  }

protected:
  SymbolInfo* find_symbol(std::string_view ident) {
    for(auto rit = _scopes.rbegin(); rit != _scopes.rend(); ++rit) {
      auto res = (*rit)->find_symbol(ident);
      if(res) return res;
    }
    return nullptr;
  }
  const SymbolInfo* find_symbol(std::string_view ident) const {
    for(auto rit = _scopes.rbegin(); rit != _scopes.rend(); ++rit) {
      auto res = (*rit)->find_symbol(ident);
      if(res) return res;
    }
    return nullptr;
  }

private:
  std::vector<Scope*> _scopes;
};

// collect struct, enum, const item and type alias.
class TypeDeclarator : public ScopedVisitor {
public:
  TypeDeclarator(ErrorRecorder *recorder, sem_type::TypePool *pool)
  : _recorder(recorder), _pool(pool) {}

  void preVisit(StructStruct &node) override {
    auto symbol = find_symbol(node.ident());
    if(!symbol) {
      _recorder->report("Symbol not found for StructStruct");
      return;
    }
    symbol->type = _pool->make_type<sem_type::StructType>(node.ident());
  }

  void preVisit(Enumeration &node) override {
    auto symbol = find_symbol(node.ident());
    if(!symbol) {
      _recorder->report("Symbol not found for Enumeration");
      return;
    }
    symbol->type = _pool->make_type<sem_type::EnumType>(node.ident());
  }

  void preVisit(ConstantItem &node) override {
    auto symbol = find_symbol(node.ident());
    if(!symbol) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    symbol->is_const = true;
  }

  void preVisit(TypeAlias &node) override {
    auto symbol = find_symbol(node.ident());
    if(!symbol) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    // set type later
  }

private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_pool;
};

// fill the struct, enum, const and alias types.
class TypeFiller : public ScopedVisitor {
public:
  TypeFiller(ErrorRecorder *recorder, sem_type::TypePool *pool)
  : _recorder(recorder), _pool(pool) {}

  void preVisit(Crate &node) override {
    ScopedVisitor::preVisit(node);
    node.scope()->load_primitive(_pool);
  }

  void postVisit(StructStruct &node) override {
    auto info = find_symbol(node.ident());
    if(!info || info->kind != SymbolKind::kStruct || !info->type) {
      _recorder->report("Struct symbol not filled");
      return;
    }
    std::map<std::string_view, sem_type::TypePtr> struct_fields;
    if(node.fields_opt()) {
      for(const auto &field: node.fields_opt()->fields()) {
        auto ident = field->ident();
        auto ast_type = field->type()->get_type();
        if(!ast_type) {
          _recorder->report("Unresolved struct field type");
          continue; // continue partial compiling
        }
        struct_fields.emplace(ident, ast_type);
      }
    }
    info->type.get<sem_type::StructType>()->set_fields(std::move(struct_fields));
    node.set_type(info->type);
  }

  void preVisit(Enumeration &node) override {

  }

  void preVisit(ConstantItem &node) override {

  }

  void preVisit(TypeAlias &node) override {

  }

private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_pool;
};


}


#endif // INSOMNIA_SYNTAX_CHECK_H













