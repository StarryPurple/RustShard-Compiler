#ifndef INSOMNIA_SYNTAX_CHECK_H
#define INSOMNIA_SYNTAX_CHECK_H

#include <unordered_map>

#include "ast_recursive_visitor.h"
#include "ast_type.h"
#include "parser.h"

namespace insomnia::rust_shard::ast {

// check branch syntax (break/continue/return), set scopes and collect symbols
class SymbolCollector : public RecursiveVisitor {
  static const std::string kDuplicateDefinitionErr, kLoopErr;
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
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for StructStruct");
      return;
    }
    info->type = _pool->make_type<sem_type::StructType>(node.ident());
    node.set_type(info->type);
  }

  void preVisit(Enumeration &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Enumeration");
      return;
    }
    info->type = _pool->make_type<sem_type::EnumType>(node.ident());
    if(node.items_opt()) {
      for(auto &item: node.items_opt()->items()) {
        item->set_type(info->type);
      }
    }
    node.set_type(info->type);
  }

  void preVisit(ConstantItem &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    info->is_const = true;
    node.set_type(info->type);
  }

  void preVisit(TypeAlias &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    node.set_type(info->type);
  }

  void preVisit(Trait &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Trait");
      return;
    }
    info->type = _pool->make_type<sem_type::TraitType>(node.ident());
    node.set_type(info->type);
  }

private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_pool;
};

// evaluate const items.
// if evaluation fails, the ConstValue in the expression will not be set.
class ConstEvaluator: public RecursiveVisitor {
  static const std::string kErrTag;
public:
  ConstEvaluator(ErrorRecorder *recorder, sem_type::TypePool *type_pool, sem_const::ConstPool *const_pool)
  : _recorder(recorder), _type_pool(type_pool), _const_pool(const_pool) {}

  void postVisit(LiteralExpression &node);
  void postVisit(BorrowExpression &node);
  void postVisit(DereferenceExpression &node);
  void postVisit(NegationExpression &node);
  void postVisit(ArithmeticOrLogicalExpression &node);
  void postVisit(ComparisonExpression &node);
  void visit(LazyBooleanExpression &node);
  void postVisit(TypeCastExpression &node);
  void postVisit(GroupedExpression &node);
  void postVisit(ArrayExpression &node) {
    _recorder->report("Array expression consteval is not supported");
  }
  void postVisit(IndexExpression &node) {
    _recorder->report("Index expression consteval is not supported");
  }
  void postVisit(TupleExpression &node) {
    _recorder->report("Tuple expression consteval is not supported");
  }
  void postVisit(TupleIndexingExpression &node) {
    _recorder->report("Tuple index expression consteval is not supported");
  }
  void postVisit(StructExpression &node) {
    _recorder->report("Struct expression consteval is not supported");
  }
  void postVisit(FieldExpression &node) {
    _recorder->report("Field expression consteval is not supported");
  }
  void postVisit(ContinueExpression &node) {
    _recorder->report("Continue expression consteval is not supported");
  }
  void postVisit(BreakExpression &node) {
    _recorder->report("Break expression consteval is not supported");
  }
  void postVisit(AssignmentExpression &node) {
    _recorder->report("Assignment consteval is not supported");
  }
  void postVisit(CompoundAssignmentExpression &node) {
    _recorder->report("Compound assignment consteval is not supported");
  }
  void postVisit(CallExpression &node) {
    _recorder->report("Function consteval is not supported");
  }
  void postVisit(MethodCallExpression &node) {
    _recorder->report("Method function consteval is not supported");
  }
private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_type_pool;
  sem_const::ConstPool *_const_pool;
};

// fill the struct, enum, const and alias types.
class TypeFiller : public ScopedVisitor {
private:
  void constEvaluate(Expression &node) {
  }
public:
  TypeFiller(ErrorRecorder *recorder, sem_type::TypePool *pool)
  : _recorder(recorder), _pool(pool) {}

  void preVisit(Crate &node) override {
    ScopedVisitor::preVisit(node);
    node.scope()->load_primitive(_pool);
  }

  void postVisit(StructStruct &node) override {
    auto info = find_symbol(node.ident());
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
  }

  void postVisit(Enumeration &node) override {
    auto info = find_symbol(node.ident());
    std::map<std::string_view, std::pair<sem_type::TypePtr, std::int64_t>> enum_variants;
    std::int64_t dis = 0;
    if(node.items_opt()) {
      for(const auto &item: node.items_opt()->items()) {
        if(item->discr_opt()) {
          _recorder->report("EnumItemDiscrimination not implemented");
        }
        enum_variants.emplace(
          item->ident(),
          std::pair(_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kI64), dis)
        );
        ++dis;
      }
    }
    info->type.get<sem_type::EnumType>()->set_variants(std::move(enum_variants));
  }

  void postVisit(ConstantItem &node) override {
    auto info = find_symbol(node.ident());
  }

  void postVisit(TypeAlias &node) override {

  }

  void postVisit(Trait &node) override;

private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_pool;
};


}


#endif // INSOMNIA_SYNTAX_CHECK_H













