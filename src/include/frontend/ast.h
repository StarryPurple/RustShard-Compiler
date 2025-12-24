/**
 * Definition of AST nodes.
 * The child-parent relationship is maintained by the parent holding unique_ptr of its child(ren).
 * In different rules, std::vector might be introduced.
 * Generally the pointers are not null.
 * If a unique_ptr can be nullptr, this property will be reflected by its name.
 * Such as:
 *   (A -> B) std::unique_ptr<B> _b;
 *   (A -> B C?) std::unique_ptr<B> _b; std::unique_ptr<C> _c_opt;
 *   // no need for _opt suffix, since std::vector has already reflected the optionality.
 *   // maybe I should change this naming rule...
 *   (A -> B*) std::vector<std::unique_ptr<B>> _b;
 *   (A -> B+) std::vector<std::unique_ptr<B>> _b;
 *   (A -> (B C)*) struct Group { B obj_b; C obj_c; }; std::vector<Group> _groups;
 */
// the builtin type for TUPLE_INDEX is not fixed... Maybe fix it someday.
#ifndef RUST_SHARD_FRONTEND_AST_H
#define RUST_SHARD_FRONTEND_AST_H

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "constant.h"
#include "fwd.h"
#include "defs.h"
#include "visitor.h"

namespace insomnia::rust_shard::ast {

#define EXPOSE_FIELD_CONST_REFERENCE(func_name, field_name) \
  auto func_name() const -> const decltype(field_name) & { return field_name; }

class Crate : public BasicNode, public ScopeInfo, public ResolutionInfo {
public:
  explicit Crate(std::vector<std::unique_ptr<Item>> &&items) : _items(std::move(items)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::string _ident;
  std::vector<std::unique_ptr<Item>> _items;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
  EXPOSE_FIELD_CONST_REFERENCE(items, _items)

  void set_name(std::string ident) { _ident = std::move(ident); }
};

class Item : public BasicNode {
public:
  explicit Item(std::unique_ptr<VisItem> &&vis_item) : _vis_item(std::move(vis_item)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<VisItem> _vis_item;
public:
  EXPOSE_FIELD_CONST_REFERENCE(vis_item, _vis_item)
};

class VisItem : public BasicNode {
public:
  VisItem() = default;
private:
  // Intentional blank.
};

class Function : public VisItem, public TypeInfo, public ResolutionInfo {
public:
  Function(
    bool is_const,
    StringRef ident,
    std::unique_ptr<FunctionParameters> &&params_opt,
    std::unique_ptr<Type> &&res_type_opt,
    std::unique_ptr<FunctionBodyExpr> &&body_opt
  ): _is_const(is_const), _ident(ident), _params_opt(std::move(params_opt)),
  _res_type_opt(std::move(res_type_opt)), _body_opt(std::move(body_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_const;
  StringRef _ident;
  std::unique_ptr<FunctionParameters> _params_opt;
  std::unique_ptr<Type> _res_type_opt;
  std::unique_ptr<FunctionBodyExpr> _body_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_const, _is_const)
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
  EXPOSE_FIELD_CONST_REFERENCE(params_opt, _params_opt)
  EXPOSE_FIELD_CONST_REFERENCE(res_type_opt, _res_type_opt)
  EXPOSE_FIELD_CONST_REFERENCE(body_opt, _body_opt)
};

class FunctionParameters : public BasicNode {
public:
  FunctionParameters(
    std::unique_ptr<SelfParam> &&self_param,
    std::vector<std::unique_ptr<FunctionParam>> &&func_params
  ): _self_param_opt(std::move(self_param)), _func_params(std::move(func_params)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<SelfParam> _self_param_opt;
  std::vector<std::unique_ptr<FunctionParam>> _func_params;
public:
  EXPOSE_FIELD_CONST_REFERENCE(self_param_opt, _self_param_opt)
  EXPOSE_FIELD_CONST_REFERENCE(func_params, _func_params)
};

class FunctionParam : public BasicNode {
public:
  FunctionParam() = default;
  virtual bool has_name() = 0;
private:
  // Intentional blank.
public:
  // some helpers.
};

class FunctionParamPattern : public FunctionParam {
public:
  FunctionParamPattern(
    std::unique_ptr<PatternNoTopAlt> &&pattern,
    std::unique_ptr<Type> &&type
  ): _pattern(std::move(pattern)), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool has_name() override { return true; }
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type;
public:
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
};

class FunctionParamType : public FunctionParam {
public:
  explicit FunctionParamType(
    std::unique_ptr<Type> &&type
  ): _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool has_name() override { return false; }
private:
  std::unique_ptr<Type> _type;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
};

class SelfParam : public BasicNode {
public:
  SelfParam(
    bool is_ref, bool is_mut,
    std::unique_ptr<Type> &&type
  ): _is_ref(is_ref), _is_mut(is_mut), _type_opt(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_ref;
  bool _is_mut;
  std::unique_ptr<Type> _type_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_ref, _is_ref)
  EXPOSE_FIELD_CONST_REFERENCE(is_mut, _is_mut)
  EXPOSE_FIELD_CONST_REFERENCE(type_opt, _type_opt)
};

class Type : public BasicNode, public TypeInfo {
public:
  Type() = default;
private:
  // Intentional blank.
};

class TypeNoBounds : public Type {
public:
  TypeNoBounds() = default;
private:
  // Intentional blank.
};

class ParenthesizedType : public TypeNoBounds {
public:
  ParenthesizedType(
    std::unique_ptr<Type> &&type
  ): _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Type> _type;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
};

class TupleType : public TypeNoBounds {
public:
  TupleType(
    std::vector<std::unique_ptr<Type>> &&types
  ): _types(std::move(types)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Type>> _types;
public:
  EXPOSE_FIELD_CONST_REFERENCE(types, _types)
};

class ReferenceType : public TypeNoBounds {
public:
  ReferenceType(
    bool is_mut,
    std::unique_ptr<TypeNoBounds> &&type_no_bounds
  ): _is_mut(is_mut), _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_mut;
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_mut, _is_mut)
  EXPOSE_FIELD_CONST_REFERENCE(type_no_bounds, _type_no_bounds)
};

class ArrayType : public TypeNoBounds {
public:
  ArrayType(
    std::unique_ptr<Type> &&type,
    std::unique_ptr<Expression> &&const_expr
  ): _type(std::move(type)), _const_expr(std::move(const_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Type> _type;
  std::unique_ptr<Expression> _const_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
  EXPOSE_FIELD_CONST_REFERENCE(const_expr, _const_expr)
};

class SliceType : public TypeNoBounds {
public:
  explicit SliceType(
    std::unique_ptr<Type> &&type
  ): _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Type> _type;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
};

class Struct : public VisItem, public TypeInfo {
public:
  Struct() = default;
private:
  // Intentional blank.
};

class StructStruct : public Struct {
public:
  StructStruct(
    StringRef struct_name,
    std::unique_ptr<StructFields> &&fields_opt
  ): _struct_name(struct_name), _fields_opt(std::move(fields_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _struct_name;
  std::unique_ptr<StructFields> _fields_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _struct_name)
  EXPOSE_FIELD_CONST_REFERENCE(fields_opt, _fields_opt)
};

class StructFields : public BasicNode {
public:
  explicit StructFields(
    std::vector<std::unique_ptr<StructField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<StructField>> _fields;
public:
  EXPOSE_FIELD_CONST_REFERENCE(fields, _fields)
};

class StructField : public BasicNode {
public:
  StructField(
    StringRef struct_name,
    std::unique_ptr<Type> &&type
  ): _struct_name(struct_name), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _struct_name;
  std::unique_ptr<Type> _type;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _struct_name)
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
};

class Enumeration : public VisItem, public TypeInfo {
public:
  Enumeration(
    StringRef enum_name,
    std::unique_ptr<EnumItems> &&items_opt
  ): _enum_name(enum_name), _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _enum_name;
  std::unique_ptr<EnumItems> _items_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _enum_name)
  EXPOSE_FIELD_CONST_REFERENCE(items_opt, _items_opt)
};

class EnumItems : public BasicNode {
public:
  explicit EnumItems(
    std::vector<std::unique_ptr<EnumItem>> &&item
  ): _items(std::move(item)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<EnumItem>> _items;
public:
  EXPOSE_FIELD_CONST_REFERENCE(items, _items)
};

class EnumItem : public BasicNode, public TypeInfo {
public:
  EnumItem(
    StringRef item_name,
    std::unique_ptr<EnumItemDiscriminant> &&discr_opt
  ): _item_name(item_name), _discr_opt(std::move(discr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _item_name;
  std::unique_ptr<EnumItemDiscriminant> _discr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _item_name)
  EXPOSE_FIELD_CONST_REFERENCE(discr_opt, _discr_opt)
};

class EnumItemDiscriminant : public BasicNode {
public:
  explicit EnumItemDiscriminant(
    std::unique_ptr<Expression> &&const_expr
  ): _const_expr(std::move(const_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _const_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(const_expr, _const_expr)
};

class ConstantItem : public VisItem, public TypeInfo {
public:
  ConstantItem(
    StringRef item_name,
    std::unique_ptr<Type> &&type,
    std::unique_ptr<Expression> &&const_expr_opt
  ): _item_name(item_name), _type(std::move(type)), _const_expr_opt(std::move(const_expr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _item_name; // might be an underscore.
  std::unique_ptr<Type> _type;
  std::unique_ptr<Expression> _const_expr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _item_name)
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
  EXPOSE_FIELD_CONST_REFERENCE(const_expr_opt, _const_expr_opt)
};

class Trait : public VisItem, public TypeInfo, public ScopeInfo {
public:
  Trait(
    StringRef trait_name,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _trait_name(trait_name), _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _trait_name;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _trait_name)
  EXPOSE_FIELD_CONST_REFERENCE(asso_items, _asso_items)
};

class AssociatedItem : public BasicNode {
public:
  AssociatedItem() = default;
private:
  // Intentional blank.
};

class AssociatedTypeAlias : public AssociatedItem {
public:
  explicit AssociatedTypeAlias(
    std::unique_ptr<TypeAlias> &&alias
  ): _alias(std::move(alias)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<TypeAlias> _alias;
public:
  EXPOSE_FIELD_CONST_REFERENCE(alias, _alias)
};

class AssociatedConstantItem : public AssociatedItem {
public:
  explicit AssociatedConstantItem(
    std::unique_ptr<ConstantItem> &&citem
  ): _citem(std::move(citem)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<ConstantItem> _citem;
public:
  EXPOSE_FIELD_CONST_REFERENCE(citem, _citem)
};

class AssociatedFunction : public AssociatedItem {
public:
  explicit AssociatedFunction(
    std::unique_ptr<Function> &&func
  ): _func(std::move(func)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Function> _func;
public:
  EXPOSE_FIELD_CONST_REFERENCE(func, _func)
};

class TypeAlias : public VisItem, public TypeInfo {
public:
  TypeAlias(
    StringRef alias_name,
    std::unique_ptr<Type> &&type_opt
  ): _alias_name(alias_name), _type_opt(std::move(type_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _alias_name;
  std::unique_ptr<Type> _type_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _alias_name)
  EXPOSE_FIELD_CONST_REFERENCE(type_opt, _type_opt)
};

class Implementation : public VisItem, public ScopeInfo {
public:
  Implementation() = default;
private:
  // Intentional blank.
};

class InherentImpl : public Implementation {
public:
  InherentImpl(
    std::unique_ptr<Type> &&type,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _type(std::move(type)), _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Type> _type;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type, _type)
  EXPOSE_FIELD_CONST_REFERENCE(asso_items, _asso_items)
};

class TraitImpl : public Implementation {
public:
  TraitImpl(
    std::unique_ptr<TypePath> &&type_path,
    std::unique_ptr<Type> &&tar_type,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _type_path(std::move(type_path)), _tar_type(std::move(tar_type)),
  _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<TypePath> _type_path;
  std::unique_ptr<Type> _tar_type;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
public:
  EXPOSE_FIELD_CONST_REFERENCE(type_path, _type_path)
  EXPOSE_FIELD_CONST_REFERENCE(tar_type, _tar_type)
  EXPOSE_FIELD_CONST_REFERENCE(asso_items, _asso_items)
};

class TypePath : public TypeNoBounds {
public:
  explicit TypePath(
    bool is_absolute,
    std::vector<std::unique_ptr<TypePathSegment>> &&segments
  ): _is_absolute(is_absolute), _segments(std::move(segments)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_absolute;
  std::vector<std::unique_ptr<TypePathSegment>> _segments;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_absolute, _is_absolute);
  EXPOSE_FIELD_CONST_REFERENCE(segments, _segments)
};

class TypePathSegment : public BasicNode {
public:
  TypePathSegment(
    std::unique_ptr<PathIdentSegment> &&ident_segment
  ): _ident_segment(std::move(ident_segment)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PathIdentSegment> _ident_segment;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident_segment, _ident_segment)
};

class PathIdentSegment : public BasicNode {
public:
  explicit PathIdentSegment(
    StringRef ident
  ): _ident(ident) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _ident; // might be super/self/Self/crate/$crate
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
};

class ConstEvaluator;

class Expression : public BasicNode, public TypeInfo {
  friend ConstEvaluator;
public:
  Expression() = default;
  virtual bool has_block() const = 0;
  bool has_constant() const { return static_cast<bool>(_cval); }
  bool is_lside() const { return _is_lside; }
  void set_lside() { _is_lside = true; }
  bool is_place_mut() const { return _is_place_mut; }
  void set_place_mut() { _is_place_mut = true; }
  bool always_returns() const { return _always_returns; }
  void set_always_returns() { _always_returns = true; }
  virtual bool allow_auto_deref() const { return false; }
protected:
  sconst::ConstValPtr _cval;
private:
  bool _is_lside = false;
  bool _is_place_mut = false;
  bool _always_returns = false;

  // for ConstEvaluator
  void set_cval(sconst::ConstValPtr value) {
    _cval = std::move(value);
  }
  /*
  template <class T>
  void set_cval(sem_type::TypePtr type, const T &value) {
    (*_cval)->set(std::move(type), value);
  }
  */
public:
  EXPOSE_FIELD_CONST_REFERENCE(cval, _cval);
};

class ExpressionWithoutBlock : public Expression {
public:
  ExpressionWithoutBlock() = default;
  bool has_block() const override { return false; }
private:
  // Intentional blank.
};

class LiteralExpression : public ExpressionWithoutBlock {
public:
  // must be assigned at construction
  template <class Spec> requires type_utils::is_primitive<Spec>
  explicit LiteralExpression(stype::TypePrime prime, Spec &&spec)
  : _prime(prime), _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  stype::TypePrime _prime;
  type_utils::primitive_variant _spec;
public:
  EXPOSE_FIELD_CONST_REFERENCE(prime, _prime)
  EXPOSE_FIELD_CONST_REFERENCE(spec_value, _spec)
};

class PathExpression : public ExpressionWithoutBlock {
public:
  PathExpression() = default;
private:
  // Intentional blank.
};

class PathInExpression : public PathExpression {
public:
  explicit PathInExpression(
    std::vector<std::unique_ptr<PathExprSegment>> &&segments
  ): _segments(std::move(segments)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<PathExprSegment>> _segments;
public:
  EXPOSE_FIELD_CONST_REFERENCE(segments, _segments)
};

class PathExprSegment : public BasicNode {
public:
  explicit PathExprSegment(
    std::unique_ptr<PathIdentSegment> &&ident_seg
  ): _ident_seg(std::move(ident_seg)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PathIdentSegment> _ident_seg;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident_seg, _ident_seg)
};

class OperatorExpression : public ExpressionWithoutBlock {
public:
  OperatorExpression() = default;
private:
  // Intentional blank.
};

// Always 1 ref.
class BorrowExpression : public OperatorExpression {
public:
  BorrowExpression(
    bool is_mut,
    std::unique_ptr<Expression> &&expr
  ): _is_mut(is_mut), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_mut;
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_mut, _is_mut)
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class DereferenceExpression : public OperatorExpression {
public:
  explicit DereferenceExpression(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class NegationExpression : public OperatorExpression {
public:
  NegationExpression(
    Operator oper,
    std::unique_ptr<Expression> &&expr
  ): _oper(oper), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  Operator _oper;
  std::unique_ptr<Expression> _expr;
public:
  // kSub/kLogicalNot
  EXPOSE_FIELD_CONST_REFERENCE(oper, _oper)
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class ArithmeticOrLogicalExpression : public OperatorExpression {
public:
  ArithmeticOrLogicalExpression(
  Operator oper,
  std::unique_ptr<Expression> &&expr1,
  std::unique_ptr<Expression> &&expr2
): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  Operator _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  // kAdd/kSub/kMul/kDiv/kMod/kBitwiseAnd/kBitwiseOr/kBitwiseXor/kShl/kShr
  EXPOSE_FIELD_CONST_REFERENCE(oper, _oper)
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class ComparisonExpression : public OperatorExpression {
public:
  ComparisonExpression(
  Operator oper,
  std::unique_ptr<Expression> &&expr1,
  std::unique_ptr<Expression> &&expr2
): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  Operator _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  // kEq/kNe/kGt/kLt/kGe/kLe
  EXPOSE_FIELD_CONST_REFERENCE(oper, _oper)
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class LazyBooleanExpression : public OperatorExpression {
public:
  LazyBooleanExpression(
    Operator oper,
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  Operator _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  // kLogicalAnd/kLogicalOr
  EXPOSE_FIELD_CONST_REFERENCE(oper, _oper)
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class TypeCastExpression : public OperatorExpression {
public:
  TypeCastExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<TypeNoBounds> &&type_no_bounds
  ): _expr(std::move(expr)), _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(type_no_bounds, _type_no_bounds)
};

class AssignmentExpression : public OperatorExpression {
public:
  AssignmentExpression(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class CompoundAssignmentExpression : public OperatorExpression {
public:
  CompoundAssignmentExpression(
    Operator oper,
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  Operator _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  // kAddAssign/kSubAssign/kMulAssign/kDivAssign/kModAssign/
  // kBitwiseAndAssign/kBitwiseOrAssign/kBitwiseXorAssign/
  // kShlAssign/kShrAssign
  EXPOSE_FIELD_CONST_REFERENCE(oper, _oper);
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class GroupedExpression : public ExpressionWithoutBlock {
public:
  GroupedExpression(
    std::unique_ptr<Expression> expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class ArrayExpression : public ExpressionWithoutBlock {
public:
  explicit ArrayExpression(
    std::unique_ptr<ArrayElements> &&elements_opt
  ): _elements_opt(std::move(elements_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<ArrayElements> _elements_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(elements_opt, _elements_opt)
};

class ArrayElements : public BasicNode {
public:
  ArrayElements() = default;
  virtual bool is_explicit() = 0;
private:
  // Intentional blank.
};

class ExplicitArrayElements : public ArrayElements {
public:
  explicit ExplicitArrayElements(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _expr_list(std::move(expr_list)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool is_explicit() override { return true; }
private:
  std::vector<std::unique_ptr<Expression>> _expr_list;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_list, _expr_list)
};

class RepeatedArrayElements : public ArrayElements {
public:
  RepeatedArrayElements(
    std::unique_ptr<Expression> &&val_expr,
    std::unique_ptr<Expression> &&len_expr
  ): _val_expr(std::move(val_expr)), _len_expr(std::move(len_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool is_explicit() override { return false; }
private:
  std::unique_ptr<Expression> _val_expr, _len_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(val_expr, _val_expr)
  EXPOSE_FIELD_CONST_REFERENCE(len_expr, _len_expr)
};

class IndexExpression : public ExpressionWithoutBlock {
public:
  IndexExpression(
    std::unique_ptr<Expression> &&expr_obj,
    std::unique_ptr<Expression> &&expr_index
  ): _expr_obj(std::move(expr_obj)), _expr_index(std::move(expr_index)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool allow_auto_deref() const override { return true; }
private:
  std::unique_ptr<Expression> _expr_obj;
  std::unique_ptr<Expression> _expr_index;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_obj, _expr_obj)
  EXPOSE_FIELD_CONST_REFERENCE(expr_index, _expr_index)
};

class TupleExpression : public ExpressionWithoutBlock {
public:
  explicit TupleExpression(
    std::unique_ptr<TupleElements> &&elems_opt
  ): _elems_opt(std::move(elems_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<TupleElements> _elems_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(elems_opt, _elems_opt)
};

class TupleElements : public BasicNode {
public:
  explicit TupleElements(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _expr_list(std::move(expr_list)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Expression>> _expr_list;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_list, _expr_list)
};

class TupleIndexingExpression : public ExpressionWithoutBlock {
public:
  TupleIndexingExpression(
    std::unique_ptr<Expression> &&expr,
    std::uint64_t index
  ): _expr(std::move(expr)), _index(index) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
  std::uint64_t _index;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(index, _index)
};

class StructExpression : public ExpressionWithoutBlock {
public:
  StructExpression(
    std::unique_ptr<PathInExpression> &&path,
    std::unique_ptr<StructExprFields> &&fields_opt
  ): _path(std::move(path)), _fields_opt(std::move(fields_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PathInExpression> _path;
  std::unique_ptr<StructExprFields> _fields_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(path, _path)
  EXPOSE_FIELD_CONST_REFERENCE(fields_opt, _fields_opt)
};

class StructExprFields : public BasicNode {
public:
  explicit StructExprFields(
    std::vector<std::unique_ptr<StructExprField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<StructExprField>> _fields;
public:
  EXPOSE_FIELD_CONST_REFERENCE(fields, _fields)
};

class StructExprField : public BasicNode {
public:
  StructExprField() = default;
  virtual bool is_named() const = 0;
private:
  // Intentional blank.
};

class NamedStructExprField : public StructExprField {
public:
  NamedStructExprField(
    StringRef ident,
    std::unique_ptr<Expression> &&expr
  ): _ident(ident), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool is_named() const override { return true; }
private:
  StringRef _ident;
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class IndexStructExprField : public StructExprField {
public:
  IndexStructExprField(
    std::uint64_t index,
    std::unique_ptr<Expression> &&expr
  ): _index(index), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool is_named() const override { return false; }
private:
  std::uint64_t _index;
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(index, _index)
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class CallExpression : public ExpressionWithoutBlock {
public:
  CallExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<CallParams> &&params_opt
  ): _expr(std::move(expr)), _params_opt(std::move(params_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<CallParams> _params_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(params_opt, _params_opt)
};

class CallParams : public BasicNode {
public:
  explicit CallParams(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _expr_list(std::move(expr_list)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Expression>> _expr_list;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_list, _expr_list)
};

class MethodCallExpression : public ExpressionWithoutBlock {
public:
  MethodCallExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<PathExprSegment> &&segment,
    std::unique_ptr<CallParams> &&params_opt
  ): _expr(std::move(expr)), _segment(std::move(segment)),
  _params_opt(std::move(params_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<PathExprSegment> _segment;
  std::unique_ptr<CallParams> _params_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(segment, _segment)
  EXPOSE_FIELD_CONST_REFERENCE(params_opt, _params_opt)
};

class FieldExpression : public ExpressionWithoutBlock {
public:
  FieldExpression(
    std::unique_ptr<Expression> &&expr,
    StringRef ident
  ): _expr(std::move(expr)), _ident(ident) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
  bool allow_auto_deref() const override { return true; }
private:
  std::unique_ptr<Expression> _expr;
  StringRef _ident;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
};

class ContinueExpression : public ExpressionWithoutBlock {
public:
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  // Intentional blank.
};

class BreakExpression : public ExpressionWithoutBlock {
public:
  BreakExpression(
    std::unique_ptr<Expression> &&expr_opt
  ): _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_opt, _expr_opt)
};

class RangeExpression : public ExpressionWithoutBlock {
public:
  RangeExpression() = default;
private:
  // Intentional blank.
};

class RangeExpr : public RangeExpression {
public:
  RangeExpr(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class RangeFromExpr : public RangeExpression {
public:
  explicit RangeFromExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class RangeToExpr : public RangeExpression {
public:
  explicit RangeToExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class RangeFullExpr : public RangeExpression {
public:
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  // Intentional blank.
};

class RangeInclusiveExpr : public RangeExpression {
public:
  RangeInclusiveExpr(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr1, _expr2;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr1, _expr1)
  EXPOSE_FIELD_CONST_REFERENCE(expr2, _expr2)
};

class RangeToInclusiveExpr : public RangeExpression {
public:
  explicit RangeToInclusiveExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class ReturnExpression : public ExpressionWithoutBlock {
public:
  explicit ReturnExpression(
    std::unique_ptr<Expression> &&expr_opt
  ): _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr_opt, _expr_opt)
};

class UnderscoreExpression : public ExpressionWithoutBlock {
public:
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  // Intentional blank.
};

class ExpressionWithBlock : public Expression {
public:
  ExpressionWithBlock() = default;
  bool has_block() const override { return true; }
private:
  // Intentional blank.
};

class BlockExpression : public ExpressionWithBlock, public ScopeInfo {
public:
  explicit BlockExpression(
    std::unique_ptr<Statements> &&stmts_opt
  ): _stmts_opt(std::move(stmts_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Statements> _stmts_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(stmts_opt, _stmts_opt)
};

class FunctionBodyExpr : public ExpressionWithBlock, public ScopeInfo {
public:
  explicit FunctionBodyExpr(
    std::unique_ptr<Statements> &&stmts_opt
  ): _stmts_opt(std::move(stmts_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Statements> _stmts_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(stmts_opt, _stmts_opt)

private:
  std::vector<ReturnExpression*> _func_returns;
public:
  void add_return_expr(ReturnExpression *node) { _func_returns.push_back(node); }
  EXPOSE_FIELD_CONST_REFERENCE(func_returns, _func_returns);
};

class Statements : public BasicNode {
public:
  Statements(
    std::vector<std::unique_ptr<Statement>> &&stmts,
    std::unique_ptr<Expression> &&expr_opt
  ): _stmts(std::move(stmts)), _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Statement>> _stmts;
  std::unique_ptr<Expression> _expr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(stmts, _stmts)
  EXPOSE_FIELD_CONST_REFERENCE(expr_opt, _expr_opt)
};

class Statement : public BasicNode {
public:
  Statement() = default;
private:
  // Intentional blank.
};

class EmptyStatement : public Statement {
public:
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  // Intentional blank.
};

class ItemStatement : public Statement {
public:
  explicit ItemStatement(
    std::unique_ptr<Item> &&item
  ): _item(std::move(item)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Item> _item;
public:
  EXPOSE_FIELD_CONST_REFERENCE(item, _item)
};

class LetStatement : public Statement {
public:
  LetStatement(
    std::unique_ptr<PatternNoTopAlt> &&pattern,
    std::unique_ptr<Type> &&type_opt,
    std::unique_ptr<Expression> &&expr_opt
  ): _pattern(std::move(pattern)), _type_opt(std::move(type_opt)),
  _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type_opt;
  std::unique_ptr<Expression> _expr_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
  EXPOSE_FIELD_CONST_REFERENCE(type_opt, _type_opt)
  EXPOSE_FIELD_CONST_REFERENCE(expr_opt, _expr_opt)
};

class ExpressionStatement : public Statement {
public:
  explicit ExpressionStatement(
    std::unique_ptr<Expression> &&expr,
    bool has_semi
  ): _expr(std::move(expr)), _has_semi(has_semi) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
  bool _has_semi;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(has_semi, _has_semi)
  std::unique_ptr<Expression> release_expr() { return std::move(_expr); }
};

class LoopExpression : public ExpressionWithBlock {
public:
  LoopExpression() = default;
private:
  std::vector<BreakExpression*> _loop_breaks;
public:
  void add_break_expr(BreakExpression *node) { _loop_breaks.push_back(node); }
  EXPOSE_FIELD_CONST_REFERENCE(loop_breaks, _loop_breaks);
};

class InfiniteLoopExpression : public LoopExpression {
public:
  explicit InfiniteLoopExpression(
    std::unique_ptr<BlockExpression> &&block_expr
  ): _block_expr(std::move(block_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<BlockExpression> _block_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(block_expr, _block_expr)
};

class PredicateLoopExpression : public LoopExpression {
public:
  PredicateLoopExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Conditions> _cond;
  std::unique_ptr<BlockExpression> _block_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(cond, _cond)
  EXPOSE_FIELD_CONST_REFERENCE(block_expr, _block_expr)
};

class IfExpression : public ExpressionWithBlock {
public:
  IfExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)),
  _else_spec() {}
  IfExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr,
    std::unique_ptr<BlockExpression> &&else_block_expr
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)),
  _else_spec(std::move(else_block_expr)) {}
  IfExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr,
    std::unique_ptr<IfExpression> &&if_expr
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)),
  _else_spec(std::move(if_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Conditions> _cond;
  std::unique_ptr<BlockExpression> _block_expr;
  std::variant<
    std::monostate,
    std::unique_ptr<BlockExpression>,
    std::unique_ptr<IfExpression>
  > _else_spec;
public:
  EXPOSE_FIELD_CONST_REFERENCE(cond, _cond)
  EXPOSE_FIELD_CONST_REFERENCE(block_expr, _block_expr)
  EXPOSE_FIELD_CONST_REFERENCE(else_spec, _else_spec)
};

class Conditions : public BasicNode {
public:
  explicit Conditions(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr; // not StructExpression
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class MatchExpression : public ExpressionWithBlock {
public:
  MatchExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<MatchArms> &&match_arms_opt
  ): _expr(std::move(expr)), _match_arms_opt(std::move(match_arms_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr; // not StructExpression
  std::unique_ptr<MatchArms> _match_arms_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
  EXPOSE_FIELD_CONST_REFERENCE(match_arms_opt, _match_arms_opt)
};

class MatchArms : public BasicNode {
public:
  explicit MatchArms(
    std::vector<std::pair<std::unique_ptr<MatchArm>, std::unique_ptr<Expression>>> &&arms
  ): _arms(std::move(arms)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::pair<std::unique_ptr<MatchArm>, std::unique_ptr<Expression>>> _arms;
public:
  EXPOSE_FIELD_CONST_REFERENCE(arms, _arms)
};

class MatchArm : public BasicNode, public ScopeInfo {
public:
  MatchArm(
    std::unique_ptr<Pattern> &&pattern,
    std::unique_ptr<MatchArmGuard> &&guard_opt
  ): _pattern(std::move(pattern)), _guard_opt(std::move(guard_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Pattern> _pattern;
  std::unique_ptr<MatchArmGuard> _guard_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
  EXPOSE_FIELD_CONST_REFERENCE(guard_opt, _guard_opt)
};

class MatchArmGuard : public BasicNode {
public:
  explicit MatchArmGuard(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Expression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class Pattern : public BasicNode {
public:
  explicit Pattern(
    std::vector<std::unique_ptr<PatternNoTopAlt>> &&patterns
  ): _patterns(std::move(patterns)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<PatternNoTopAlt>> _patterns;
public:
  EXPOSE_FIELD_CONST_REFERENCE(patterns, _patterns)
};

class PatternNoTopAlt : public BasicNode {
public:
  PatternNoTopAlt() = default;
private:
  // Intentional blank.
};

class PatternWithoutRange : public PatternNoTopAlt {
public:
  PatternWithoutRange() = default;
private:
  // Intentional blank.
};

class LiteralPattern : public PatternWithoutRange {
public:
  LiteralPattern(
    bool is_neg,
    std::unique_ptr<LiteralExpression> &&expr
  ): _is_neg(is_neg), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_neg;
  std::unique_ptr<LiteralExpression> _expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_neg, _is_neg)
  EXPOSE_FIELD_CONST_REFERENCE(expr, _expr)
};

class IdentifierPattern : public PatternWithoutRange {
public:
  IdentifierPattern(
    bool is_ref,
    bool is_mut,
    StringRef ident,
    std::unique_ptr<PatternNoTopAlt> &&pattern_opt
  ): _is_ref(is_ref), _is_mut(is_mut), _ident(ident),
  _pattern_opt(std::move(pattern_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_ref;
  bool _is_mut;
  StringRef _ident;
  std::unique_ptr<PatternNoTopAlt> _pattern_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_ref, _is_ref)
  EXPOSE_FIELD_CONST_REFERENCE(is_mut, _is_mut)
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
  EXPOSE_FIELD_CONST_REFERENCE(pattern_opt, _pattern_opt)
};

class WildcardPattern : public PatternWithoutRange {
public:
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  // Intentional blank.
};

class ReferencePattern : public PatternWithoutRange {
public:
  ReferencePattern(
    bool is_mut,
    std::unique_ptr<PatternWithoutRange> &&pattern
  ): _is_mut(is_mut), _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  bool _is_mut;
  std::unique_ptr<PatternWithoutRange> _pattern;
public:
  EXPOSE_FIELD_CONST_REFERENCE(is_mut, _is_mut)
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
};

class StructPattern : public PatternWithoutRange {
public:
  StructPattern(
    std::unique_ptr<PathInExpression> &&path_in_expr,
    std::unique_ptr<StructPatternElements> &&elems_opt
  ): _path_in_expr(std::move(path_in_expr)),
  _elems_opt(std::move(elems_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PathInExpression> _path_in_expr;
  std::unique_ptr<StructPatternElements> _elems_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(path_in_expr, _path_in_expr)
  EXPOSE_FIELD_CONST_REFERENCE(elems_opt, _elems_opt)
};

class StructPatternElements : public BasicNode {
public:
  explicit StructPatternElements(
    std::unique_ptr<StructPatternFields> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<StructPatternFields> _fields;
public:
  EXPOSE_FIELD_CONST_REFERENCE(fields, _fields)
};

class StructPatternFields : public BasicNode {
public:
  explicit StructPatternFields(
    std::vector<std::unique_ptr<StructPatternField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<StructPatternField>> _fields;
public:
  EXPOSE_FIELD_CONST_REFERENCE(fields, _fields)
};

class StructPatternField : public BasicNode {
public:
  StructPatternField(
    StringRef ident,
    std::unique_ptr<Pattern> &&pattern
  ): _ident(ident), _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  StringRef _ident;
  std::unique_ptr<Pattern> _pattern;
public:
  EXPOSE_FIELD_CONST_REFERENCE(ident, _ident)
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
};

class TuplePattern : public PatternWithoutRange {
public:
  explicit TuplePattern(
    std::unique_ptr<TuplePatternItems> items_opt
  ): _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<TuplePatternItems> _items_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(items_opt, _items_opt)
};

class TuplePatternItems : public BasicNode {
public:
  explicit TuplePatternItems(
    std::vector<std::unique_ptr<Pattern>> &&patterns
  ): _patterns(std::move(patterns)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Pattern>> _patterns;
public:
  EXPOSE_FIELD_CONST_REFERENCE(patterns, _patterns)
};

class GroupedPattern : public PatternWithoutRange {
public:
  explicit GroupedPattern(
    std::unique_ptr<Pattern> &&pattern
  ): _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<Pattern> _pattern;
public:
  EXPOSE_FIELD_CONST_REFERENCE(pattern, _pattern)
};

class SlicePattern : public PatternWithoutRange {
public:
  explicit SlicePattern(
    std::unique_ptr<SlicePatternItems> &&items_opt
  ): _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<SlicePatternItems> _items_opt;
public:
  EXPOSE_FIELD_CONST_REFERENCE(items_opt, _items_opt)
};

class SlicePatternItems : public BasicNode {
public:
  explicit SlicePatternItems(
    std::vector<std::unique_ptr<Pattern>> &&patterns
  ): _patterns(std::move(patterns)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::vector<std::unique_ptr<Pattern>> _patterns;
public:
  EXPOSE_FIELD_CONST_REFERENCE(patterns, _patterns)
};

class PathPattern : public PatternWithoutRange {
public:
  explicit PathPattern(
    std::unique_ptr<PathExpression> &&path_expr
  ): _path_expr(std::move(path_expr)) {}
  void accept(BasicVisitor &visitor) override { visitor.visit(*this); }
private:
  std::unique_ptr<PathExpression> _path_expr;
public:
  EXPOSE_FIELD_CONST_REFERENCE(path_expr, _path_expr)
};
}

#undef EXPOSE_FIELD_CONST_REFERENCE
#endif // RUST_SHARD_FRONTEND_AST_H