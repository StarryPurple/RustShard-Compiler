#ifndef INSOMNIA_SYNTAX_CHECK_H
#define INSOMNIA_SYNTAX_CHECK_H

#include <unordered_map>

#include "ast_recursive_visitor.h"
#include "ast_type.h"
#include "parser.h"

namespace insomnia::rust_shard::ast {

// check branch syntax (break/continue/return) and collect them.
// set scopes and collect symbols (for all vis items, not variables)
// After this, all scopes shall be settled.
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
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction
    }); // add to outer scope
    if(!info)
      _recorder->report("Function symbol already defined: " + std::string(node.ident()));
    _scopes.push_back(std::make_unique<Scope>());
  }
  void postVisit(Function &node) override {
    if(node.body_opt())
      node.body_opt()->set_scope(std::move(_scopes.back()));
    _scopes.pop_back();
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
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTrait
    });
    if(!info)
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
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kConstant
    });
    if(!info)
      _recorder->report("ConstantItem symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(StructStruct &node) override {
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kStruct
    });
    if(!info)
      _recorder->report("StructStruct symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(Enumeration &node) override {
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kEnum
    });
    if(!info)
      _recorder->report("Enumeration symbol already defined: " + std::string(node.ident()));
  }
  void preVisit(IdentifierPattern &node) override {
    // no, not now. later checked in TypeFiller.
    /*
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kVariable
    });
    if(!info)
      _recorder->report("Variable symbol already defined: " + std::string(node.ident()));
    */
  }
  void preVisit(TypeAlias &node) override {
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kTypeAlias
    });
    if(!info)
      _recorder->report("TypaAlias typename already used: " + std::string(node.ident()));
  }

  // type related:
  // struct(dealt), enum(dealt), type alias(dealt)

  // loop check: (continue, break)
  void preVisit(FunctionBodyExpr &node) override {
    _func_context.push_back(&node);
  }
  void postVisit(FunctionBodyExpr &node) override {
    _func_context.pop_back();
  }
  void preVisit(InfiniteLoopExpression &node) override {
    _loop_context.push_back(&node);
  }
  void postVisit(InfiniteLoopExpression &node) override {
    _loop_context.pop_back();
  }
  void preVisit(PredicateLoopExpression &node) override {
    _loop_context.push_back(&node);
  }
  void postVisit(PredicateLoopExpression &node) override {
    _loop_context.pop_back();
  }
  void preVisit(BreakExpression &node) override {
    if(_loop_context.empty()) {
      _recorder->report("break expression not inside a loop.");
      return;
    }
    _loop_context.back()->add_break_expr(&node);
  }

  void preVisit(ContinueExpression &node) override {
    if(_loop_context.empty())
      _recorder->report("continue expression not inside a loop.");
    // the loop does not need continue expressions
  }

  // function check: (return)
  void preVisit(ReturnExpression &node) override {
    if(_func_context.empty()) {
      _recorder->report("return expression not inside a function.");
      return;
    }
    _func_context.back()->add_return_expr(&node);
  }

private:
  ErrorRecorder *_recorder;
  std::vector<std::unique_ptr<Scope>> _scopes; // store the constructing scopes
  std::vector<LoopExpression*> _loop_context;
  std::vector<FunctionBodyExpr*> _func_context;

  SymbolInfo* add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
};

// Automatically enters/exits scopes.
// Affected:
// preVisit, postVisit: Crate, BlockExpression, Function, InherentImpl, TraitImpl, Trait
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
  SymbolInfo* add_symbol(std::string_view ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
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

// Collect struct, enum, const item and type alias.
// After this, all types (including builtin ones) shall be registered in symbol type pool
// (incomplete. some relationships not filled.) Including: builtin primitive types, structs,
// enumeration, enumeration item (singleton type), type aliases, and traits.
// (enumeration/enumeration_item relationship has been filled.)
// The type filling progress is ready to launch.
class TypeDeclarator : public ScopedVisitor {
public:
  TypeDeclarator(ErrorRecorder *recorder, sem_type::TypePool *type_pool)
  : _recorder(recorder), _type_pool(type_pool) {}

  void preVisit(Crate &node) override {
    ScopedVisitor::preVisit(node);
    node.scope()->load_builtin_types(_type_pool);
  }

  void preVisit(StructStruct &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for StructStruct");
      return;
    }
    info->type = _type_pool->make_type<sem_type::StructType>(node.ident());
    node.set_type(info->type);
  }

  void preVisit(Enumeration &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Enumeration");
      return;
    }
    info->type = _type_pool->make_type<sem_type::EnumType>(node.ident());
    node.set_type(info->type);
    auto enum_ptr = info->type.get<sem_type::EnumType>();

    // types of enum items
    sem_type::EnumType::variant_map_t enum_variants;
    if(node.items_opt()) {
      sem_type::EnumVariantType::discriminant_t dis = 0;
      for(const auto &item: node.items_opt()->items()) {
        if(item->discr_opt()) {
          _recorder->report("EnumItemDiscrimination not implemented. Ignoring it");
        }
        auto enum_variant_type = _type_pool->make_type<sem_type::EnumVariantType>(
          item->ident(), dis, std::vector<sem_type::TypePtr>(), enum_ptr
        );
        item->set_type(enum_variant_type); // each enum item _singleton_ has its distinct type
        enum_variants.emplace(
          item->ident(), enum_variant_type.get<sem_type::EnumVariantType>()
        );
        ++dis;
      }
    }
    enum_ptr->set_variants(std::move(enum_variants));
  }

  void preVisit(TypeAlias &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    info->type = _type_pool->make_type<sem_type::AliasType>(node.ident());
    node.set_type(info->type);
  }

  void preVisit(Trait &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Trait");
      return;
    }
    info->type = _type_pool->make_type<sem_type::TraitType>(node.ident());
    node.set_type(info->type);
  }

private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_type_pool;
};

// A helper class that evaluates const items.
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

#define ISM_RS_POST_VISIT_OVERRIDE_METHOD(Node) \
  void postVisit(Node &node) override;

// fill the struct, enum, const and alias types.
class TypeFiller : public ScopedVisitor {
  static const std::string
  kErrTypeNotResolved, kErrTypeNotMatch, kErrConstevalFailed,
  kErrIdentNotResolved;
  bool constEvaluate(Expression &node) {
    if(node.has_constant()) return true;
    node.accept(_evaluator);
    if(!node.has_constant()) {
      _recorder->report("const evaluation failed");
      return false;
    }
    return true;
  }
public:
  TypeFiller(ErrorRecorder *recorder, sem_type::TypePool *type_pool, sem_const::ConstPool *const_pool)
  : _recorder(recorder), _type_pool(type_pool), _const_pool(const_pool),
  _evaluator(recorder, type_pool, const_pool) {}

  // decide the types

  INSOMNIA_RUST_SHARD_AST_TYPED_VISITABLE_NODES_LIST(ISM_RS_POST_VISIT_OVERRIDE_METHOD);

  // set the types
  void postVisit(LetStatement &node) override;

  // helpers
  // If type not match, binding will fail

  void bind_pattern(PatternNoTopAlt *pattern, sem_type::TypePtr type);
  void bind_identifier(IdentifierPattern *pattern, sem_type::TypePtr type);
  void bind_wildcard(WildcardPattern *pattern, sem_type::TypePtr type);
  void bind_tuple(TuplePattern *pattern, sem_type::TypePtr type);
  void bind_struct(StructPattern *pattern, sem_type::TypePtr type);
  void bind_reference(ReferencePattern *pattern, sem_type::TypePtr type);
  void bind_literal(LiteralPattern *pattern, sem_type::TypePtr type);
  void bind_grouped(GroupedPattern *pattern, sem_type::TypePtr type);
  void bind_slice(SlicePattern *pattern, sem_type::TypePtr type);
  void bind_path(PathPattern *pattern, sem_type::TypePtr type);


private:
  ErrorRecorder *_recorder;
  sem_type::TypePool *_type_pool;
  sem_const::ConstPool *_const_pool;
  ConstEvaluator _evaluator;
};

#undef ISM_RS_POST_VISIT_OVERRIDE_METHOD

}


#endif // INSOMNIA_SYNTAX_CHECK_H













