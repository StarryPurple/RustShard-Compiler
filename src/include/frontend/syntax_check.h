#ifndef RUST_SHARD_FRONTEND_SYNTAX_CHECK_H
#define RUST_SHARD_FRONTEND_SYNTAX_CHECK_H

#include <unordered_map>
#include <utility>

#include "recursive_visitor.h"
#include "type.h"
#include "parser.h"

namespace insomnia::rust_shard::ast {

/* Check branch syntax (break/continue/return) and collect them.
 * set scopes and collect symbols (for all vis items, not variables)
 * After this, all scopes shall be settled.
 */
class SymbolCollector : public RecursiveVisitor {
  static const std::string kDuplicateDefinitionErr, kControlStatementErr, kScopeErr;
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
    /* No, not here. Later in PreTypeFiller.
    auto info = add_symbol(node.ident(), SymbolInfo{
      .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction
    }); // add to outer scope
    if(!info)
      _recorder->report("Function symbol already defined: " + std::string(node.ident()));
    */
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

  SymbolInfo* add_symbol(StringRef ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
};

/* Automatically enters/exits scopes & Collect module references.
 * Affected:
 * preVisit, postVisit: Crate, BlockExpression, Function, InherentImpl, TraitImpl, Trait
 * visit: MatchArms,
 */
class ScopedVisitor : public RecursiveVisitor {
public:
  ScopedVisitor() = default;

  void preVisit(Crate &node) override {
    _scopes.push_back(node.scope().get());
    _crate = &node;
  }
  void postVisit(Crate &node) override {
    _scopes.pop_back();
    _crate = nullptr;
    if(!_scopes.empty())
      throw std::runtime_error("Scope management broke down");
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
    _impl_type = node.type()->get_type();
    _is_in_asso_block = true;
  }
  void postVisit(InherentImpl &node) override {
    _scopes.pop_back();
    _impl_type = stype::TypePtr();
    _is_in_asso_block = false;
  }
  void preVisit(TraitImpl &node) override {
    _scopes.push_back(node.scope().get());
    _is_in_asso_block = true;
  }
  void postVisit(TraitImpl &node) override {
    _scopes.pop_back();
    _is_in_asso_block = false;
  }
  void preVisit(Trait &node) override {
    _scopes.push_back(node.scope().get());
    _is_in_asso_block = true;
  }
  void postVisit(Trait &node) override {
    _scopes.pop_back();
    _is_in_asso_block = false;
  }

protected:
  std::vector<Scope*> _scopes;
  // "Self" that we are currently working on. valid in impl/trait.
  // set in PreTypeFiller.
  stype::TypePtr _impl_type;

  SymbolInfo* add_symbol(StringRef ident, const SymbolInfo &symbol) {
    return _scopes.back()->add_symbol(ident, symbol);
  }
  SymbolInfo* find_symbol(StringRef ident) {
    for(auto rit = _scopes.rbegin(); rit != _scopes.rend(); ++rit) {
      auto res = (*rit)->find_symbol(ident);
      if(res) return res;
    }
    return nullptr;
  }
  const SymbolInfo* find_symbol(StringRef ident) const {
    for(auto rit = _scopes.rbegin(); rit != _scopes.rend(); ++rit) {
      auto res = (*rit)->find_symbol(ident);
      if(res) return res;
    }
    return nullptr;
  }

  bool is_in_asso_block() const { return _is_in_asso_block; }

  void add_asso_method(stype::TypePtr caller_type, std::shared_ptr<stype::FunctionType> func_type) {
    _crate->add_asso_method(std::move(caller_type), std::move(func_type));
  }
  std::shared_ptr<stype::FunctionType> find_asso_method(
    stype::TypePtr caller_type, const StringRef &func_ident, stype::TypePool *pool) {
    return _crate->find_asso_method(std::move(caller_type), func_ident, pool);
  }

private:
  // inherent impl, trait impl, trait
  bool _is_in_asso_block = false;
  Crate *_crate;
};

/* Collect struct, enum, const item and type alias.
 * After this, all types (including builtin ones) shall be registered in symbol type pool
 * (incomplete. some relationships not filled.) Including: builtin primitive types, structs,
 * enumeration, enumeration item (singleton type), type aliases, traits.
 * (enumeration/enumeration_item relationship has been filled.)
 * The type filling progress is ready to launch.
 */
class TypeDeclarator : public ScopedVisitor {
public:
  TypeDeclarator(ErrorRecorder *recorder, stype::TypePool *type_pool)
  : _recorder(recorder), _type_pool(type_pool) {}

  void preVisit(Crate &node) override {
    ScopedVisitor::preVisit(node);
    load_builtin(&node);
  }

  /* not here.
  void preVisit(Function &node) override {
    ScopedVisitor::preVisit(node);
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Function");
      return;
    }
    info->type = _type_pool->make_type<stype::FunctionType>(node.ident());
    node.set_type(info->type);
  }
  */

  void preVisit(StructStruct &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for StructStruct");
      return;
    }
    info->type = _type_pool->make_type<stype::StructType>(node.ident());
    node.set_type(info->type);
  }

  void preVisit(Enumeration &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Enumeration");
      return;
    }
    info->type = _type_pool->make_type<stype::EnumType>(node.ident());
    node.set_type(info->type);
    auto enum_ptr = info->type.get<stype::EnumType>();

    // types of enum items
    stype::EnumType::variant_map_t enum_variants;
    if(node.items_opt()) {
      stype::EnumVariantType::discriminant_t dis = 0;
      for(const auto &item: node.items_opt()->items()) {
        if(item->discr_opt()) {
          _recorder->report("EnumItemDiscrimination not implemented. Ignoring it");
        }
        auto enum_variant_type = _type_pool->make_type<stype::EnumVariantType>(
          item->ident(), dis, std::vector<stype::TypePtr>(), enum_ptr
        );
        item->set_type(enum_variant_type); // each enum item _singleton_ has its distinct type
        enum_variants.emplace(
          item->ident(), enum_variant_type.get<stype::EnumVariantType>()
        );
        ++dis;
      }
    }
    enum_ptr->set_details(std::move(enum_variants));
  }

  void preVisit(TypeAlias &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for ConstItem");
      return;
    }
    info->type = _type_pool->make_type<stype::AliasType>(node.ident());
    node.set_type(info->type);
  }

  void preVisit(Trait &node) override {
    auto info = find_symbol(node.ident());
    if(!info) {
      _recorder->report("Symbol not found for Trait");
      return;
    }
    info->type = _type_pool->make_type<stype::TraitType>(node.ident());
    node.set_type(info->type);
  }

private:
  ErrorRecorder *_recorder;
  stype::TypePool *_type_pool;

  void load_builtin(Crate *crate);
};

/* Resolve symbol reliance relationship (used in Type Path)
 * After this, one node shall know its parent resolution region and its children.
 */
class SymbolResolver : public RecursiveVisitor {
public:
  SymbolResolver(ErrorRecorder *recorder, stype::TypePool *type_pool, ResolutionTree *res_tree)
  : _recorder(recorder), _type_pool(type_pool), _res_tree(res_tree) {}

  void postVisit(Crate &node) override {

  }

private:
  ErrorRecorder *_recorder;
  stype::TypePool *_type_pool;
  ResolutionTree *_res_tree;
};


/* A helper class that evaluates const items.
 * if evaluation fails, the ConstValue in the expression will not be set.
 */
class ConstEvaluator: public ScopedVisitor {
  static const std::string kErrTag;
public:
  ConstEvaluator(ErrorRecorder *recorder, stype::TypePool *type_pool, sconst::ConstPool *const_pool)
  : _recorder(recorder), _type_pool(type_pool), _const_pool(const_pool) {}

  bool constEvaluate(Expression &node, std::vector<Scope*> cur_scopes) {
    // fetch the scopes from the root to this expression node.
    if(node.has_constant()) return true;
    _scopes = std::move(cur_scopes);
    node.accept(*this);
    if(!node.has_constant()) {
      _recorder->report("const evaluation failed");
      return false;
    }
    return true;
  }

  // bind lvalue property
  void preVisit(AssignmentExpression &node) override;
  void preVisit(CompoundAssignmentExpression &node) override;

  void postVisit(LiteralExpression &node) override;
  void postVisit(BorrowExpression &node) override;
  void postVisit(DereferenceExpression &node) override;
  void postVisit(NegationExpression &node) override;
  void postVisit(ArithmeticOrLogicalExpression &node) override;
  void postVisit(ComparisonExpression &node) override;
  void visit(LazyBooleanExpression &node) override;
  void postVisit(TypeCastExpression &node) override;
  void postVisit(GroupedExpression &node) override;

  void postVisit(PathInExpression &node) override;
  void postVisit(ArrayExpression &node) override;
  void postVisit(IndexExpression &node) override {
    _recorder->report("Index expression consteval is not supported");
  }
  void postVisit(TupleExpression &node) override {
    _recorder->report("Tuple expression consteval is not supported");
  }
  void postVisit(TupleIndexingExpression &node) override {
    _recorder->report("Tuple index expression consteval is not supported");
  }
  void postVisit(StructExpression &node) override {
    _recorder->report("Struct expression consteval is not supported");
  }
  void postVisit(FieldExpression &node) override {
    _recorder->report("Field expression consteval is not supported");
  }
  void postVisit(ContinueExpression &node) override {
    _recorder->report("Continue expression consteval is not supported");
  }
  void postVisit(BreakExpression &node) override {
    _recorder->report("Break expression consteval is not supported");
  }
  void postVisit(AssignmentExpression &node) override {
    _recorder->report("Assignment consteval is not supported");
  }
  void postVisit(CompoundAssignmentExpression &node) override {
    _recorder->report("Compound assignment consteval is not supported");
  }
  void postVisit(CallExpression &node) override {
    _recorder->report("Function consteval is not supported");
  }
  void postVisit(MethodCallExpression &node) override {
    _recorder->report("Method function consteval is not supported");
  }
private:
  ErrorRecorder *_recorder;
  stype::TypePool *_type_pool;
  sconst::ConstPool *_const_pool;
};

/* Set all type tags (TypePath).
 * Fill empty FuncType and EnumType (TraitType?)
 */
class PreTypeFiller: public ScopedVisitor {
  static const std::string kErrTypeNotResolved, kErrTypeInvalid;
public:
  PreTypeFiller(ErrorRecorder *recorder, stype::TypePool *type_pool, sconst::ConstPool *const_pool)
  : _recorder(recorder), _type_pool(type_pool), _const_pool(const_pool),
    _evaluator(recorder, type_pool, const_pool) {}

  // for function type... and other type filling.

  void postVisit(ParenthesizedType &node) override;
  void postVisit(TupleType &node) override;
  void postVisit(ReferenceType &node) override;
  void postVisit(ArrayType &node) override;
  void postVisit(SliceType &node) override;
  void postVisit(TypePath &node) override;

  void postVisit(Function &node) override;
  // void postVisit(Enumeration &node) override;

  void postVisit(ConstantItem &node) override;

  void postVisit(SelfParam &node) override;

  void visit(InherentImpl &node) override;

  void postVisit(StructStruct &node) override;
  void postVisit(Enumeration &node) override;

private:
  ErrorRecorder *_recorder;
  stype::TypePool *_type_pool;
  sconst::ConstPool *_const_pool;
  ConstEvaluator _evaluator;
};

#define ISM_RS_POST_VISIT_OVERRIDE_METHOD(Node) \
  void postVisit(Node &node) override;

/* fill types of ast nodes.
 * fill the struct, enum, const and alias types.
 */
class TypeFiller : public ScopedVisitor {
  static const std::string
    kErrTypeNotResolved, kErrTypeNotMatch, kErrConstevalFailed,
    kErrIdentNotResolved, kErrNoPlaceMutability;
public:
  TypeFiller(ErrorRecorder *recorder, stype::TypePool *type_pool, sconst::ConstPool *const_pool)
  : _recorder(recorder), _type_pool(type_pool), _const_pool(const_pool),
  _evaluator(recorder, type_pool, const_pool) {}

  // decide the types

  RUST_SHARD_AST_TYPED_VISITABLE_NODES_LIST(ISM_RS_POST_VISIT_OVERRIDE_METHOD);

  // assignment: bind lvalue property
  void preVisit(AssignmentExpression &node) override;
  void preVisit(CompoundAssignmentExpression &node) override;
  void preVisit(IndexExpression &node) override;
  void preVisit(FieldExpression &node) override;
  void preVisit(MethodCallExpression &node) override;

  // register parameter
  void visit(Function &node) override;

  // set the types
  void postVisit(LetStatement &node) override;

  // helpers
  // If type not match, binding will fail

  void bind_pattern(PatternNoTopAlt *pattern, stype::TypePtr type, bool need_spec);
  void bind_identifier(IdentifierPattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_wildcard(WildcardPattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_tuple(TuplePattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_struct(StructPattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_reference(ReferencePattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_literal(LiteralPattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_grouped(GroupedPattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_slice(SlicePattern *pattern, stype::TypePtr type, bool need_spec);
  void bind_path(PathPattern *pattern, stype::TypePtr type, bool need_spec);


private:
  ErrorRecorder *_recorder;
  stype::TypePool *_type_pool;
  sconst::ConstPool *_const_pool;
  ConstEvaluator _evaluator;

  enum class SelfState {
    kInvalid,
    kNormal,
    kRef,
    kRefMut,
  } _self_state = SelfState::kInvalid;
};

#undef ISM_RS_POST_VISIT_OVERRIDE_METHOD

}

/* Some discarded method record method.
void add_asso_method(stype::TypePtr caller_type, std::shared_ptr<stype::FunctionType> func_type) {
    auto ident = make_inherent_method_name(caller_type, func_type);
    _scopes.front()->add_symbol(ident, SymbolInfo {
      .ident = ident,
      .kind = SymbolKind::kFunction,
      .type = stype::TypePtr(func_type),
    });
  }
  std::shared_ptr<stype::FunctionType> find_asso_method(
    stype::TypePtr caller_type, StringRef func_ident, stype::TypePool *pool) {
    auto ident = make_inherent_method_name(caller_type, func_ident);
    auto info = _scopes.front()->find_symbol(ident);
    if(info && info->kind == SymbolKind::kFunction && info->type.get_if<stype::FunctionType>()) {
      return info->type.get<stype::FunctionType>();
    }
    // special builtin mechanic: impl<T, N> [T; N]: fn len(&mut) -> usize
    // lazy generation.
    if(func_ident == "len" && caller_type.get_if<stype::ArrayType>()) {
      auto func_type = pool->make_raw_type<stype::FunctionType>(
        func_ident,
        caller_type,
        std::vector<stype::TypePtr>{},
        pool->make_type<stype::PrimeType>(stype::TypePrime::kUSize)
        );
      add_asso_method(caller_type, func_type);
    }

    return nullptr;
  }

  static std::string make_inherent_method_name(
    stype::TypePtr caller_type, std::shared_ptr<stype::FunctionType> func_type) {
    return caller_type->to_string() + "_" + func_type->ident();
  }
  static std::string make_inherent_method_name(
    stype::TypePtr caller_type, const StringRef &func_ident) {
    return caller_type->to_string() + "_" + func_ident;
  }
*/

#endif // RUST_SHARD_FRONTEND_SYNTAX_CHECK_H