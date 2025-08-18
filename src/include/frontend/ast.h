/**
 * Definition of AST nodes.
 * The child-parent relationship is maintained by the parent holding unique_ptr of its child(ren).
 * In different rules, std::vector or std::variant might be introduced.
 * Generally the pointers are not null.
 * If a unique_ptr can be nullptr, this property will be reflected by its name.
 * Such as:
 *   (A -> B) std::unique_ptr<B> _b;
 *   (A -> B C?) std::unique_ptr<B> _b; std::unique_ptr<C> _c_opt;
 *   (A -> B | C) std::variant<std::unique_ptr<B>, std::unique_ptr<C>> _spec;
 *   (A -> B (C | D)?) std::unique_ptr<B> _b;
 *      std::variant<std::monostate, std::unique_ptr<C>, std::unique_ptr<D>> _spec_opt;
 *   // no need for _opt suffix, since std::vector has already reflected the optionality.
 *   // maybe I should change this naming rule...
 *   (A -> B*) std::vector<std::unique_ptr<B>> _b;
 *   (A -> B+) std::vector<std::unique_ptr<B>> _b;
 *   (A -> (B C)*) struct Group { B b; C c; }; std::vector<Group> _groups;
 */
#ifndef INSOMNIA_AST_H
#define INSOMNIA_AST_H

#include <memory>
#include <variant>
#include <vector>

#include "ast_fwd.h"
#include "ast_type.h"
#include "ast_visitor.h"

namespace insomnia::rust_shard::ast {

class BasicNode {
public:
  virtual ~BasicNode() = default;
  virtual void accept(BasicVisitor &visitor) = 0;
};

class Crate : public BasicNode {
public:
  explicit Crate(std::vector<std::unique_ptr<Item>> &&items) : _items(std::move(items)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Item>> _items;
};

class Item : public BasicNode {
public:
  explicit Item(std::unique_ptr<VisItem> &&vis_item) : _vis_item(std::move(vis_item)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<VisItem> _vis_item;
};

class VisItem : public BasicNode {
public:
  template <class Spec>
  explicit VisItem(Spec &&spec_item): _spec_item(std::forward<Spec>(spec_item)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<Function>,
    std::unique_ptr<Struct>,
    std::unique_ptr<Enumeration>,
    std::unique_ptr<ConstantItem>,
    std::unique_ptr<Trait>,
    std::unique_ptr<Implementation>
  > _spec_item;
};

class Function : public BasicNode {
public:
  Function(
    bool is_const,
    std::string_view ident,
    std::unique_ptr<FunctionParameters> &&params_opt,
    std::unique_ptr<Type> &&res_type_opt,
    std::unique_ptr<BlockExpression> &&block_expr_opt
  ): _is_const(is_const), _ident(ident), _params_opt(std::move(params_opt)),
  _res_type_opt(std::move(res_type_opt)), _block_expr_opt(std::move(block_expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_const;
  std::string_view _ident;
  std::unique_ptr<FunctionParameters> _params_opt;
  std::unique_ptr<Type> _res_type_opt;
  std::unique_ptr<BlockExpression> _block_expr_opt;
};

class FunctionParameters : public BasicNode {
public:
  FunctionParameters(
    std::unique_ptr<SelfParam> &&self_param,
    std::vector<std::unique_ptr<FunctionParam>> &&func_params
  ): _self_param_opt(std::move(self_param)), _func_params(std::move(func_params)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<SelfParam> _self_param_opt;
  std::vector<std::unique_ptr<FunctionParam>> _func_params;
};

class FunctionParam : public BasicNode {
public:
  template <class Spec>
  explicit FunctionParam(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<FunctionParamPattern>,
    std::unique_ptr<Type>
  > _spec;
};

class FunctionParamPattern : public BasicNode {
public:
  FunctionParamPattern(
    std::unique_ptr<PatternNoTopAlt> &&pattern,
    std::unique_ptr<Type> &&type
  ): _pattern(std::move(pattern)), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type;
};

class SelfParam : public BasicNode {
public:
  SelfParam(
    bool is_ref, bool is_mut,
    std::unique_ptr<Type> &&type
  ): _is_ref(is_ref), _is_mut(is_mut), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_ref;
  bool _is_mut;
  std::unique_ptr<Type> _type;
};

class Type : public BasicNode {
public:
  Type(
    std::unique_ptr<TypeNoBounds> &&type_no_bounds
  ): _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
};

class TypeNoBounds : public BasicNode {
public:
  template <class Spec>
  explicit TypeNoBounds(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<ParenthesizedType>,
    std::unique_ptr<TupleType>,
    std::unique_ptr<ReferenceType>,
    std::unique_ptr<ArrayType>,
    std::unique_ptr<SliceType>
  > _spec;
};

class ParenthesizedType : public BasicNode {
public:
  ParenthesizedType(
    std::unique_ptr<Type> &&type
  ): _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
};

class TupleType : public BasicNode {
public:
  TupleType(
    std::vector<std::unique_ptr<Type>> &&types
  ): _types(std::move(types)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Type>> _types;
};

class ReferenceType : public BasicNode {
public:
  ReferenceType(
    bool is_mut,
    std::unique_ptr<TypeNoBounds> &&type_no_bounds
  ): _is_mut(is_mut), _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_mut;
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
};

class ArrayType : public BasicNode {
public:
  ArrayType(
    std::unique_ptr<Type> &&type,
    std::unique_ptr<Expression> &&const_expr
  ): _type(std::move(type)), _const_expr(std::move(const_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
  std::unique_ptr<Expression> _const_expr;
};

class SliceType : public BasicNode {
public:
  explicit SliceType(
    std::unique_ptr<Type> &&type
  ): _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
};

class Struct : public BasicNode {
public:
  explicit Struct(
    std::unique_ptr<StructStruct> &&ss
  ): _ss(std::move(ss)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<StructStruct> _ss;
};

class StructStruct : public BasicNode {
public:
  StructStruct(
    std::string_view struct_name,
    std::unique_ptr<StructFields> &&fields_opt
  ): _struct_name(struct_name), _fields_opt(std::move(fields_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _struct_name;
  std::unique_ptr<StructFields> _fields_opt;
};

class StructFields : public BasicNode {
public:
  explicit StructFields(
    std::vector<std::unique_ptr<StructField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<StructField>> _fields;
};

class StructField : public BasicNode {
public:
  StructField(
    std::string_view struct_name,
    std::unique_ptr<Type> &&type
  ): _struct_name(struct_name), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _struct_name;
  std::unique_ptr<Type> _type;
};

class Enumeration : public BasicNode {
public:
  Enumeration(
    std::string_view enum_name,
    std::unique_ptr<EnumItems> &&items_opt
  ): _enum_name(enum_name), _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _enum_name;
  std::unique_ptr<EnumItems> _items_opt;
};

class EnumItems : public BasicNode {
public:
  explicit EnumItems(
    std::vector<std::unique_ptr<EnumItem>> &&item
  ): _items(std::move(item)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<EnumItem>> _items;
};

class EnumItem : public BasicNode {
public:
  EnumItem(
    std::string_view item_name,
    std::unique_ptr<EnumItemDiscriminant> &&discr_opt
  ): _item_name(item_name), _discr_opt(std::move(discr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _item_name;
  std::unique_ptr<EnumItemDiscriminant> _discr_opt;
};

class EnumItemDiscriminant : public BasicNode {
public:
  explicit EnumItemDiscriminant(
    std::unique_ptr<Expression> &&const_expr
  ): _const_expr(std::move(const_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _const_expr;
};

class ConstantItem : public BasicNode {
public:
  ConstantItem(
    std::string_view item_name,
    std::unique_ptr<Type> &&type,
    std::unique_ptr<Expression> &&const_expr_opt
  ): _item_name(item_name), _type(std::move(type)), _const_expr_opt(std::move(const_expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _item_name; // might be an underscore.
  std::unique_ptr<Type> _type;
  std::unique_ptr<Expression> _const_expr_opt;
};

class Trait : public BasicNode {
public:
  Trait(
    std::string_view trait_name,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _trait_name(trait_name), _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _trait_name;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
};

class AssociatedItem : public BasicNode {
public:
  template <class Spec>
  explicit AssociatedItem(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<TypeAlias>,
    std::unique_ptr<ConstantItem>,
    std::unique_ptr<Function>
  > _spec;
};

class TypeAlias : public BasicNode {
public:
  TypeAlias(
    std::string_view alias_name,
    std::unique_ptr<Type> &&type_opt
  ): _alias_name(alias_name), _type_opt(std::move(type_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _alias_name;
  std::unique_ptr<Type> _type_opt;
};

class Implementation : public BasicNode {
public:
  template <class Spec>
  Implementation(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<InherentImpl>,
    std::unique_ptr<TraitImpl>
  > _spec;
};

class InherentImpl : public BasicNode {
public:
  InherentImpl(
    std::unique_ptr<Type> &&type,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _type(std::move(type)), _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
};

class TraitImpl : public BasicNode {
public:
  TraitImpl(
    std::unique_ptr<TypePath> &&type_path,
    std::unique_ptr<Type> &&tar_type,
    std::vector<std::unique_ptr<AssociatedItem>> &&asso_items
  ): _type_path(std::move(type_path)), _tar_type(std::move(tar_type)),
  _asso_items(std::move(asso_items)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<TypePath> _type_path;
  std::unique_ptr<Type> _tar_type;
  std::vector<std::unique_ptr<AssociatedItem>> _asso_items;
};

class TypePath : public BasicNode {
public:
  explicit TypePath(
    std::vector<std::unique_ptr<TypePathSegment>> &&segments
  ): _segments(std::move(segments)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<TypePathSegment>> _segments;
};

class TypePathSegment : public BasicNode {
public:
  TypePathSegment(
    std::unique_ptr<PathIdentSegment> &&ident_segment
  ): _ident_segment(std::move(ident_segment)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathIdentSegment> _ident_segment;
};

class PathIdentSegment : public BasicNode {
public:
  explicit PathIdentSegment(
    std::string_view ident
  ): _ident(ident) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _ident; // might be super/self/Self/crate/$crate
};

class Expression : public BasicNode {
  using ExprType = insomnia::rust_shard::type::ExprType;
public:
  template <class Spec>
  explicit Expression(Spec &&spec) : _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
  std::shared_ptr<ExprType> get_type() const { return _expr_type; }
protected:
  std::shared_ptr<ExprType> _expr_type;
private:
  std::variant<
    std::unique_ptr<ExpressionWithoutBlock>,
    std::unique_ptr<ExpressionWithBlock>
  > _spec;
};

class BasicExpression : public BasicNode {
  using ExprType = insomnia::rust_shard::type::ExprType;
public:
  BasicExpression() = default;
  void set_type(std::shared_ptr<ExprType> expr_type) { _expr_type = expr_type; }
  std::shared_ptr<ExprType> get_type() const { return _expr_type; }
private:
  std::shared_ptr<ExprType> _expr_type;
};

class ExpressionWithoutBlock : public BasicExpression {
public:
  template <class Spec>
  explicit ExpressionWithoutBlock(Spec &&spec) : _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<LiteralExpression>,
    std::unique_ptr<PathExpression>,
    std::unique_ptr<OperatorExpression>,
    std::unique_ptr<GroupedExpression>,
    std::unique_ptr<ArrayExpression>,
    std::unique_ptr<IndexExpression>,
    std::unique_ptr<TupleExpression>,
    std::unique_ptr<TupleIndexingExpression>,
    std::unique_ptr<StructExpression>,
    std::unique_ptr<CallExpression>,
    std::unique_ptr<MethodCallExpression>,
    std::unique_ptr<FieldExpression>,
    std::unique_ptr<ContinueExpression>,
    std::unique_ptr<BreakExpression>,
    std::unique_ptr<RangeExpression>,
    std::unique_ptr<ReturnExpression>,
    std::unique_ptr<UnderscoreExpression>
  > _spec;
};

class LiteralExpression : public BasicExpression {
  using TypePrime = insomnia::rust_shard::type::TypePrime;
public:
  template <class Spec>
  explicit LiteralExpression(TypePrime prime, Spec &&spec)
  : _prime(prime), _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  TypePrime _prime;
  std::variant<
    char,
    std::string_view,
    std::int64_t,
    std::uint64_t,
    float,
    double,
    bool
  > _spec;
};

class PathExpression : public BasicExpression {
public:
  explicit PathExpression(
    std::unique_ptr<PathInExpression> &&path
  ): _path(std::move(path)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathInExpression> _path;
};

class PathInExpression : public BasicExpression {
public:
  explicit PathInExpression(
    std::vector<std::unique_ptr<PathExprSegment>> &&segments
  ): _segments(std::move(segments)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<PathExprSegment>> _segments;
};

class PathExprSegment : public BasicExpression {
public:
  explicit PathExprSegment(
    std::unique_ptr<PathIdentSegment> &&ident
  ): _ident(std::move(ident)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathIdentSegment> _ident;
};

class OperatorExpression : public BasicExpression {
public:
  template <class Spec>
  OperatorExpression(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<BorrowExpression>,
    std::unique_ptr<DereferenceExpression>,
    std::unique_ptr<NegationExpression>,
    std::unique_ptr<ArithmeticOrLogicalExpression>,
    std::unique_ptr<ComparisonExpression>,
    std::unique_ptr<LazyBooleanExpression>,
    std::unique_ptr<TypeCastExpression>,
    std::unique_ptr<AssignmentExpression>,
    std::unique_ptr<CompoundAssignmentExpression>
  > _spec;
};

class BorrowExpression : public BasicExpression {
public:
  BorrowExpression(
    int ref_cnt,
    bool is_mut,
    std::unique_ptr<Expression> &&expr
  ): _ref_cnt(ref_cnt), _is_mut(is_mut), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  int _ref_cnt; // 0/1/2
  bool _is_mut;
  std::unique_ptr<Expression> _expr;
};

class DereferenceExpression : public BasicExpression {
public:
  explicit DereferenceExpression(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class NegationExpression : public BasicExpression {
public:
  NegationExpression(
    bool is_neg,
    std::unique_ptr<Expression> &&expr
  ): _is_neg(is_neg), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_neg; // true for neg '-', false for not '!'
  std::unique_ptr<Expression> _expr;
};

class ArithmeticOrLogicalExpression : public BasicExpression {
public:
  ArithmeticOrLogicalExpression(
  std::string_view oper,
  std::unique_ptr<Expression> &&expr1,
  std::unique_ptr<Expression> &&expr2
): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
};

class ComparisonExpression : public BasicExpression {
public:
  ComparisonExpression(
  std::string_view oper,
  std::unique_ptr<Expression> &&expr1,
  std::unique_ptr<Expression> &&expr2
): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
};

class LazyBooleanExpression : public BasicExpression {
public:
  LazyBooleanExpression(
    std::string_view oper,
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _oper(oper), _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::string_view _oper;
  std::unique_ptr<Expression> _expr1, _expr2;
};

class TypeCastExpression : public BasicExpression {
public:
  TypeCastExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<TypeNoBounds> &&type_no_bounds
  ): _expr(std::move(expr)), _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
};

class AssignmentExpression : public BasicExpression {
public:
  AssignmentExpression(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr1, _expr2;
};

class CompoundAssignmentExpression : public BasicExpression {
public:
  CompoundAssignmentExpression(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr1, _expr2;
};

class GroupedExpression : public BasicExpression {
public:
  GroupedExpression(
    std::unique_ptr<Expression> expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class ArrayExpression : public BasicExpression {
public:
  explicit ArrayExpression(
    std::unique_ptr<ArrayElements> &&elements_opt
  ): _elements_opt(std::move(elements_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<ArrayElements> _elements_opt;
};

class ArrayElements : public BasicExpression {
  enum class SpecType {
    EXPLICIT, IMPLICIT
  };
public:
  explicit ArrayElements(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _spec_type(SpecType::EXPLICIT),  _expr_list(std::move(expr_list)) {}
  ArrayElements(
    std::unique_ptr<Expression> &&rep_expr_opt,
    std::unique_ptr<Expression> &&const_expr_opt
  ): _spec_type(SpecType::IMPLICIT), _rep_expr_opt(std::move(rep_expr_opt)),
  _const_expr_opt(std::move(const_expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  SpecType _spec_type;
  std::vector<std::unique_ptr<Expression>> _expr_list;
  std::unique_ptr<Expression> _rep_expr_opt, _const_expr_opt;
};

class IndexExpression : public BasicExpression {
public:
  IndexExpression(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr_index
  ): _expr1(std::move(expr1)), _expr_index(std::move(expr_index)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr1;
  std::unique_ptr<Expression> _expr_index;
};

class TupleExpression : public BasicExpression {
public:
  explicit TupleExpression(
    std::unique_ptr<TupleElements> &&elems_opt
  ): _elems_opt(std::move(elems_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<TupleElements> _elems_opt;
};

class TupleElements : public BasicExpression {
public:
  explicit TupleElements(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _expr_list(std::move(expr_list)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Expression>> _expr_list;
};

class TupleIndexingExpression : public BasicExpression {
public:
  TupleIndexingExpression(
    std::unique_ptr<Expression> &&expr,
    std::uintptr_t index
  ): _expr(std::move(expr)), _index(index) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
  std::uintptr_t _index;
};

class StructExpression : public BasicExpression {
public:
  StructExpression(
    std::unique_ptr<PathInExpression> &&path,
    std::unique_ptr<StructExprFields> &&fields_opt
  ): _path(std::move(path)), _fields_opt(std::move(fields_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathInExpression> _path;
  std::unique_ptr<StructExprFields> _fields_opt;
};

class StructExprFields : public BasicExpression {
public:
  explicit StructExprFields(
    std::vector<std::unique_ptr<StructExprField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<StructExprField>> _fields;
};

class StructExprField : public BasicExpression {
  enum class SpecType {
    ID_ONLY, ID_EXPR, IDX_EXPR
  };
public:
  explicit StructExprField(
    std::string_view ident
  ): _spec_type(SpecType::ID_ONLY), _ident_opt(ident) {}
  StructExprField(
    std::string_view ident,
    std::unique_ptr<Expression> &&expr
  ): _spec_type(SpecType::ID_EXPR), _ident_opt(ident), _expr_opt(std::move(expr)) {}
  StructExprField(
    std::uintptr_t index,
    std::unique_ptr<Expression> &&expr
  ): _spec_type(SpecType::IDX_EXPR), _index_opt(index), _expr_opt(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  SpecType _spec_type;
  std::string_view _ident_opt;
  std::uintptr_t _index_opt{};
  std::unique_ptr<Expression> _expr_opt;
};

class CallExpression : public BasicExpression {
public:
  CallExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<CallParams> &&params_opt
  ): _expr(std::move(expr)), _params_opt(std::move(params_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<CallParams> _params_opt;
};

class CallParams : public BasicNode {
public:
  explicit CallParams(
    std::vector<std::unique_ptr<Expression>> &&expr_list
  ): _expr_list(std::move(expr_list)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Expression>> _expr_list;
};

class MethodCallExpression : public BasicExpression {
public:
  MethodCallExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<PathExprSegment> &&segment,
    std::unique_ptr<CallParams> &&params_opt
  ): _expr(std::move(expr)), _segment(std::move(segment)),
  _params_opt(std::move(params_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
  std::unique_ptr<PathExprSegment> _segment;
  std::unique_ptr<CallParams> _params_opt;
};

class FieldExpression : public BasicExpression {
public:
  FieldExpression(
    std::unique_ptr<Expression> &&expr,
    std::string_view ident
  ): _expr(std::move(expr)), _ident(ident) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
  std::string_view _ident;
};

class ContinueExpression : public BasicExpression {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class BreakExpression : public BasicExpression {
public:
  BreakExpression(
    std::unique_ptr<Expression> &&expr_opt
  ): _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr_opt;
};

class RangeExpression : public BasicExpression {
public:
  template <class Spec>
  explicit RangeExpression(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<RangeExpr>,
    std::unique_ptr<RangeFromExpr>,
    std::unique_ptr<RangeToExpr>,
    std::unique_ptr<RangeFullExpr>,
    std::unique_ptr<RangeInclusiveExpr>,
    std::unique_ptr<RangeToInclusiveExpr>
  > _spec;
};

class RangeExpr : public BasicExpression {
public:
  RangeExpr(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr1, _expr2;
};

class RangeFromExpr : public BasicExpression {
public:
  explicit RangeFromExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class RangeToExpr : public BasicExpression {
public:
  explicit RangeToExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class RangeFullExpr : public BasicExpression {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class RangeInclusiveExpr : public BasicExpression {
public:
  RangeInclusiveExpr(
    std::unique_ptr<Expression> &&expr1,
    std::unique_ptr<Expression> &&expr2
  ): _expr1(std::move(expr1)), _expr2(std::move(expr2)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr1, _expr2;
};

class RangeToInclusiveExpr : public BasicExpression {
public:
  explicit RangeToInclusiveExpr(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class ReturnExpression : public BasicExpression {
public:
  explicit ReturnExpression(
    std::unique_ptr<Expression> &&expr_opt
  ): _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr_opt;
};

class UnderscoreExpression : public BasicExpression {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class ExpressionWithBlock : public BasicExpression {
public:
  template <class Spec>
  explicit ExpressionWithBlock(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<BlockExpression>,
    std::unique_ptr<LoopExpression>,
    std::unique_ptr<IfExpression>,
    std::unique_ptr<MatchExpression>
  > _spec;
};

class BlockExpression : public BasicExpression {
public:
  explicit BlockExpression(
    std::unique_ptr<Statements> &&stmts_opt
  ): _stmts_opt(std::move(stmts_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Statements> _stmts_opt;
};

class Statements : public BasicNode {
public:
  Statements(
    std::vector<std::unique_ptr<Statement>> &&stmts,
    std::unique_ptr<ExpressionWithoutBlock> &&expr_no_block_opt
  ): _stmts(std::move(stmts)), _expr_no_block_opt(std::move(expr_no_block_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Statement>> _stmts;
  std::unique_ptr<ExpressionWithoutBlock> _expr_no_block_opt;
};

class Statement : public BasicNode {
public:
  template <class Spec>
  explicit Statement(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::monostate,
    std::unique_ptr<Item>,
    std::unique_ptr<LetStatement>,
    std::unique_ptr<ExpressionStatement>
  > _spec;
};

class LetStatement : public BasicNode {
public:
  LetStatement(
    std::unique_ptr<PatternNoTopAlt> &&pattern,
    std::unique_ptr<Type> &&type_opt,
    std::unique_ptr<Expression> &&expr_opt
  ): _pattern(std::move(pattern)), _type_opt(std::move(type_opt)),
  _expr_opt(std::move(expr_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type_opt;
  std::unique_ptr<Expression> _expr_opt;
};

class ExpressionStatement : public BasicNode {
public:
  template <class Spec>
  explicit ExpressionStatement(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<ExpressionWithoutBlock>,
    std::unique_ptr<ExpressionWithBlock>
  > _spec;
};

class LoopExpression : public BasicExpression {
public:
  template <class Spec>
  explicit LoopExpression(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<InfiniteLoopExpression>,
    std::unique_ptr<PredicateLoopExpression>
  > _spec;
};

class InfiniteLoopExpression : public BasicExpression {
public:
  explicit InfiniteLoopExpression(
    std::unique_ptr<BlockExpression> &&block_expr
  ): _block_expr(std::move(block_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<BlockExpression> _block_expr;
};

class PredicateLoopExpression : public BasicExpression {
public:
  PredicateLoopExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Conditions> _cond;
  std::unique_ptr<BlockExpression> _block_expr;
};

class IfExpression : public BasicExpression {
public:
  template <class ElseSpec>
  IfExpression(
    std::unique_ptr<Conditions> &&cond,
    std::unique_ptr<BlockExpression> &&block_expr,
    ElseSpec &&else_spec
  ): _cond(std::move(cond)), _block_expr(std::move(block_expr)),
  _else_spec(std::forward<ElseSpec>(else_spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Conditions> _cond;
  std::unique_ptr<BlockExpression> _block_expr;
  std::variant<
    std::monostate,
    std::unique_ptr<BlockExpression>,
    std::unique_ptr<IfExpression>
  > _else_spec;
};

class Conditions : public BasicNode {
public:
  explicit Conditions(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr; // not StructExpression
};

class MatchExpression : public BasicExpression {
public:
  MatchExpression(
    std::unique_ptr<Expression> &&expr,
    std::unique_ptr<MatchArms> &&match_arms_opt
  ): _expr(std::move(expr)), _match_arms_opt(std::move(match_arms_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr; // not StructExpression
  std::unique_ptr<MatchArms> _match_arms_opt;
};

class MatchArms : public BasicNode {
public:
  explicit MatchArms(
    std::vector<std::pair<std::unique_ptr<MatchArm>, std::unique_ptr<Expression>>> &&arms
  ): _arms(std::move(arms)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::pair<std::unique_ptr<MatchArm>, std::unique_ptr<Expression>>> _arms;
};

class MatchArm : public BasicNode {
public:
  MatchArm(
    std::unique_ptr<Pattern> &&pattern,
    std::unique_ptr<MatchArmGuard> &&guard_opt
  ): _pattern(std::move(pattern)), _guard_opt(std::move(guard_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Pattern> _pattern;
  std::unique_ptr<MatchArmGuard> _guard_opt;
};

class MatchArmGuard : public BasicNode {
public:
  explicit MatchArmGuard(
    std::unique_ptr<Expression> &&expr
  ): _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Expression> _expr;
};

class Pattern : public BasicNode {
public:
  explicit Pattern(
    std::vector<std::unique_ptr<PatternNoTopAlt>> &&pattern_list
  ): _pattern_list(std::move(pattern_list)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<PatternNoTopAlt>> _pattern_list;
};

class PatternNoTopAlt : public BasicNode {
public:
  explicit PatternNoTopAlt(
    std::unique_ptr<PatternWithoutRange> &&pattern
  ): _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PatternWithoutRange> _pattern;
};

class PatternWithoutRange : public BasicNode {
public:
  template <class Spec>
  PatternWithoutRange(Spec &&spec): _spec(std::forward<Spec>(spec)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<LiteralPattern>,
    std::unique_ptr<IdentifierPattern>,
    std::unique_ptr<WildcardPattern>,
    std::unique_ptr<RestPattern>,
    std::unique_ptr<ReferencePattern>,
    std::unique_ptr<StructPattern>,
    std::unique_ptr<TuplePattern>,
    std::unique_ptr<GroupedPattern>,
    std::unique_ptr<SlicePattern>,
    std::unique_ptr<PathPattern>
  > _spec;
};

class LiteralPattern : public BasicNode {
public:
  LiteralPattern(
    bool is_neg,
    std::unique_ptr<LiteralExpression> &&expr
  ): _is_neg(is_neg), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_neg;
  std::unique_ptr<LiteralExpression> _expr;
};

class IdentifierPattern : public BasicNode {
public:
  IdentifierPattern(
    bool is_ref,
    bool is_mut,
    std::string_view ident,
    std::unique_ptr<PatternNoTopAlt> &&pattern_opt
  ): _is_ref(is_ref), _is_mut(is_mut), _ident(ident),
  _pattern_opt(std::move(pattern_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_ref;
  bool _is_mut;
  std::string_view _ident;
  std::unique_ptr<PatternNoTopAlt> _pattern_opt;
};

class WildcardPattern : public BasicNode {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class RestPattern : public BasicNode {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class ReferencePattern : public BasicNode {
public:
  ReferencePattern(
    int ref_cnt,
    bool is_mut,
    std::unique_ptr<PatternWithoutRange> &&pattern
  ): _ref_cnt(ref_cnt), _is_mut(is_mut), _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override;
private:
  int _ref_cnt;
  bool _is_mut;
  std::unique_ptr<PatternWithoutRange> _pattern;
};

class StructPattern : public BasicNode {
public:
  StructPattern(
    std::unique_ptr<PathInExpression> &&path_in_expr,
    std::unique_ptr<StructPatternElements> &&elems_opt
  ): _path_in_expr(std::move(path_in_expr)),
  _elems_opt(std::move(elems_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathInExpression> _path_in_expr;
  std::unique_ptr<StructPatternElements> _elems_opt;
};

class StructPatternElements : public BasicNode {
  enum class Type {
    NORMAL, ET_CETERA
  };
public:
  StructPatternElements(
    std::unique_ptr<StructPatternFields> &&fields,
    std::unique_ptr<StructPatternEtCetera> &&etc_opt
  ): _type(Type::NORMAL), _fields(std::move(fields)),
  _etc_opt(std::move(etc_opt)) {}
  StructPatternElements(
    std::unique_ptr<StructPatternEtCetera> &&etc
  ): _type(Type::ET_CETERA), _etc_opt(std::move(etc)) {}
  void accept(BasicVisitor &visitor) override;
private:
  Type _type;
  std::unique_ptr<StructPatternFields> _fields;
  std::unique_ptr<StructPatternEtCetera> _etc_opt;
};

class StructPatternEtCetera : public BasicNode {
public:
  void accept(BasicVisitor &visitor) override;
private:
  // Intended blank
};

class StructPatternFields : public BasicNode {
public:
  explicit StructPatternFields(
    std::vector<std::unique_ptr<StructPatternField>> &&fields
  ): _fields(std::move(fields)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<StructPatternField>> _fields;
};

class StructPatternField : public BasicNode {
  enum class SpecType {
    INTEGER, IDENTIFIER, RESTRICTION
  };
public:
  StructPatternField(
    std::uintptr_t index,
    std::unique_ptr<Pattern> &&pattern
  ): _spec_type(SpecType::INTEGER),
  _index(index), _pattern_opt(std::move(pattern)) {}
  StructPatternField(
    std::string_view ident,
    std::unique_ptr<Pattern> &&pattern
  ): _spec_type(SpecType::IDENTIFIER),
  _ident(ident), _pattern_opt(std::move(pattern)) {}
  StructPatternField(
    bool is_ref,
    bool is_mut,
    std::string_view ident
  ): _spec_type(SpecType::RESTRICTION),
  _is_ref(is_ref), _is_mut(is_mut), _ident(ident) {}
  void accept(BasicVisitor &visitor) override;
private:
  SpecType _spec_type;
  std::uintptr_t _index;
  std::string_view _ident;
  bool _is_ref, _is_mut;
  std::unique_ptr<Pattern> _pattern_opt;
};

class TuplePattern : public BasicNode {
public:
  explicit TuplePattern(
    std::unique_ptr<TuplePatternItems> items_opt
  ): _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<TuplePatternItems> _items_opt;
};

class TuplePatternItems : public BasicNode {
  enum class SpecType {
    PATTERNS, REST
  };
public:
  explicit TuplePatternItems(
    std::vector<std::unique_ptr<Pattern>> &&patterns
  ): _spec_type(SpecType::PATTERNS), _patterns(std::move(patterns)) {}
  explicit TuplePatternItems(
    std::unique_ptr<RestPattern> &&rest
  ): _spec_type(SpecType::REST), _rest_opt(std::move(rest)) {}
  void accept(BasicVisitor &visitor) override;
private:
  SpecType _spec_type;
  std::vector<std::unique_ptr<Pattern>> _patterns;
  std::unique_ptr<RestPattern> _rest_opt;
};

class GroupedPattern : public BasicNode {
public:
  explicit GroupedPattern(
    std::unique_ptr<Pattern> &&pattern
  ): _pattern(std::move(pattern)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Pattern> _pattern;
};

class SlicePattern : public BasicNode {
public:
  explicit SlicePattern(
    std::unique_ptr<SlicePatternItems> &&items_opt
  ): _items_opt(std::move(items_opt)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<SlicePatternItems> _items_opt;
};

class SlicePatternItems : public BasicNode {
public:
  explicit SlicePatternItems(
    std::vector<std::unique_ptr<Pattern>> &&patterns
  ): _patterns(std::move(patterns)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Pattern>> _patterns;
};

class PathPattern : public BasicNode {
public:
  explicit PathPattern(
    std::unique_ptr<PathExpression> &&path_expr
  ): _path_expr(std::move(path_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PathExpression> _path_expr;
};

}

#endif // INSOMNIA_AST_H