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
#include "ast_visitor.h"

namespace insomnia::ast {

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
    std::string_view fn_name,
    std::unique_ptr<FunctionParameters> &&params,
    std::unique_ptr<Type> &&res_type,
    std::unique_ptr<BlockExpression> &&block_expr
  ): _is_const(is_const), _fn_name(fn_name), _params_opt(std::move(params)),
  _res_type_opt(std::move(res_type)), _block_expr_opt(std::move(block_expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_const;
  std::string_view _fn_name;
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
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ExpressionWithoutBlock : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class LiteralExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PathExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PathInExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PathExprSegment : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class OperatorExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class BorrowExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class DereferenceExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class NegationExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ArithmeticOrLogicalExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ComparisonExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class LazyBooleanExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TypeCastExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class AssignmentExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class CompoundAssignmentExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class GroupedExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ArrayExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ArrayElements : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class IndexExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TupleExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TupleElements : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TupleIndexingExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructExprFields : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructExprField : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructBase : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class CallExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class CallParams : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class MethodCallExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class FieldExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ContinueExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class BreakExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangFromExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeToExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeFullExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeInclusiveExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RangeToInclusiveExpr : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ReturnExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class UnderscoreExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ExpressionWithBlock : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class BlockExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class Statements : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class Statement : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class LetStatement : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ExpressionStatement : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class LoopExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class InfiniteLoopExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PredicateLoopExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class IfExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class Conditions : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class MatchExpression : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class MatchArms : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class MatchArm : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class MatchArmGuard : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class Pattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PatternNoTopAlt : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PatternWithoutRange : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class LiteralPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class IdentifierPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class WildcardPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class RestPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class ReferencePattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructPatternElements : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructPatternEtCetera : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructPatternFields : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class StructPatternField : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TuplePattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class TuplePatternItems : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class GroupedPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class SlicePattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class SlicePatternItems : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};

class PathPattern : public BasicNode {
public:

  void accept(BasicVisitor &visitor) override;
private:

};


}

#endif // INSOMNIA_AST_H