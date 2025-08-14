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
  explicit Item(std::unique_ptr<VisItem> vis_item) : _vis_item(std::move(vis_item)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<VisItem> _vis_item;
};

class VisItem : public BasicNode {
public:
  template <class T>
  explicit VisItem(T &&spec_item) : _spec_item(std::forward<T>(spec_item)) {}
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
    std::unique_ptr<FunctionParameters> params,
    std::unique_ptr<Type> res_type,
    std::unique_ptr<BlockExpression> block_expr
    ) :
  _is_const(is_const), _fn_name(fn_name), _params_opt(std::move(params)),
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
    std::unique_ptr<SelfParam> self_param,
    std::vector<std::unique_ptr<FunctionParam>> &&func_params
  ) :
  _self_param_opt(std::move(self_param)), _func_params(std::move(func_params)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<SelfParam> _self_param_opt;
  std::vector<std::unique_ptr<FunctionParam>> _func_params;
};

class FunctionParam : public BasicNode {
public:
  template <class T>
  explicit FunctionParam(T &&spec) : _spec(std::forward<T>(spec)) {}
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
    std::unique_ptr<PatternNoTopAlt> pattern,
    std::unique_ptr<Type> type
  ) :
  _pattern(std::move(pattern)), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type;
};

class SelfParam : public BasicNode {
public:
  SelfParam(
    bool is_ref, bool is_mut,
    std::unique_ptr<Type> type
  ) :
  _is_ref(is_ref), _is_mut(is_mut), _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_ref;
  bool _is_mut;
  std::unique_ptr<Type> _type;
};

class Type : public BasicNode {
public:
  Type(
    std::unique_ptr<TypeNoBounds> type_no_bounds
  ) :
  _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
};

class TypeNoBounds : public BasicNode {
public:
  template <class T>
  explicit TypeNoBounds(T &&spec) : _spec(std::forward<T>(spec)) {}
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
    std::unique_ptr<Type> type
  ) :
  _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
};

class TupleType : public BasicNode {
public:
  TupleType(
    std::vector<std::unique_ptr<Type>> &&types
  ) :
  _types(std::move(types)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Type>> _types;
};

class ReferenceType : public BasicNode {
public:
  ReferenceType(
    bool is_mut,
    std::unique_ptr<TypeNoBounds> type_no_bounds
  ) :
  _is_mut(is_mut), _type_no_bounds(std::move(type_no_bounds)) {}
  void accept(BasicVisitor &visitor) override;
private:
  bool _is_mut;
  std::unique_ptr<TypeNoBounds> _type_no_bounds;
};

class ArrayType : public BasicNode {
public:
  ArrayType(
    std::unique_ptr<Type> type,
    std::unique_ptr<Expression> expr
  ) :
  _type(std::move(type)), _expr(std::move(expr)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
  std::unique_ptr<Expression> _expr;
};

class SliceType : public BasicNode {
public:
  SliceType(
    std::unique_ptr<Type> type
  ) :
  _type(std::move(type)) {}
  void accept(BasicVisitor &visitor) override;
private:
  std::unique_ptr<Type> _type;
};

class Struct : public BasicNode {
public:

private:

};

class StructStruct : public BasicNode {
public:

private:

};

class StructFields : public BasicNode {
public:

private:

};

class StructField : public BasicNode {
public:

private:

};

class Enumeration : public BasicNode {
public:

private:

};

class EnumItems : public BasicNode {
public:

private:

};

class EnumItem : public BasicNode {
public:

private:

};

class EnumItemDiscriminant : public BasicNode {
public:

private:

};

class ConstantItem : public BasicNode {
public:

private:

};

class Trait : public BasicNode {
public:

private:

};

class AssociatedItem : public BasicNode {
public:

private:

};

class Implementation : public BasicNode {
public:

private:

};

class InherentImpl : public BasicNode {
public:

private:

};

class TraitImpl : public BasicNode {
public:

private:

};

class TypePath : public BasicNode {
public:

private:

};

class TypePathSegment : public BasicNode {
public:

private:

};

class PathIdentSegment : public BasicNode {
public:

private:

};

class Expression : public BasicNode {
public:

private:

};

class ExpressionWithoutBlock : public BasicNode {
public:

private:

};

class LiteralExpression : public BasicNode {
public:

private:

};

class PathExpression : public BasicNode {
public:

private:

};

class PathInExpression : public BasicNode {
public:

private:

};

class PathExprSegment : public BasicNode {
public:

private:

};

class OperatorExpression : public BasicNode {
public:

private:

};

class BorrowExpression : public BasicNode {
public:

private:

};

class DereferenceExpression : public BasicNode {
public:

private:

};

class NegationExpression : public BasicNode {
public:

private:

};

class ArithmeticOrLogicalExpression : public BasicNode {
public:

private:

};

class ComparisonExpression : public BasicNode {
public:

private:

};

class LazyBooleanExpression : public BasicNode {
public:

private:

};

class TypeCastExpression : public BasicNode {
public:

private:

};

class AssignmentExpression : public BasicNode {
public:

private:

};

class CompoundAssignmentExpression : public BasicNode {
public:

private:

};

class GroupedExpression : public BasicNode {
public:

private:

};

class ArrayExpression : public BasicNode {
public:

private:

};

class ArrayElements : public BasicNode {
public:

private:

};

class IndexExpression : public BasicNode {
public:

private:

};

class TupleExpression : public BasicNode {
public:

private:

};

class TupleElements : public BasicNode {
public:

private:

};

class TupleIndexingExpression : public BasicNode {
public:

private:

};

class StructExpression : public BasicNode {
public:

private:

};

class StructExprFields : public BasicNode {
public:

private:

};

class StructExprField : public BasicNode {
public:

private:

};

class StructBase : public BasicNode {
public:

private:

};

class CallExpression : public BasicNode {
public:

private:

};

class CallParams : public BasicNode {
public:

private:

};

class MethodCallExpression : public BasicNode {
public:

private:

};

class FieldExpression : public BasicNode {
public:

private:

};

class ContinueExpression : public BasicNode {
public:

private:

};

class BreakExpression : public BasicNode {
public:

private:

};

class RangeExpression : public BasicNode {
public:

private:

};

class RangeExpr : public BasicNode {
public:

private:

};

class RangFromExpr : public BasicNode {
public:

private:

};

class RangeToExpr : public BasicNode {
public:

private:

};

class RangeFullExpr : public BasicNode {
public:

private:

};

class RangeInclusiveExpr : public BasicNode {
public:

private:

};

class RangeToInclusiveExpr : public BasicNode {
public:

private:

};

class ReturnExpression : public BasicNode {
public:

private:

};

class UnderscoreExpression : public BasicNode {
public:

private:

};

class ExpressionWithBlock : public BasicNode {
public:

private:

};

class BlockExpression : public BasicNode {
public:

private:

};

class Statements : public BasicNode {
public:

private:

};

class Statement : public BasicNode {
public:

private:

};

class LetStatement : public BasicNode {
public:

private:

};

class ExpressionStatement : public BasicNode {
public:

private:

};

class LoopExpression : public BasicNode {
public:

private:

};

class InfiniteLoopExpression : public BasicNode {
public:

private:

};

class PredicateLoopExpression : public BasicNode {
public:

private:

};

class IfExpression : public BasicNode {
public:

private:

};

class Conditions : public BasicNode {
public:

private:

};

class MatchExpression : public BasicNode {
public:

private:

};

class MatchArms : public BasicNode {
public:

private:

};

class MatchArm : public BasicNode {
public:

private:

};

class MatchArmGuard : public BasicNode {
public:

private:

};

class Pattern : public BasicNode {
public:

private:

};

class PatternNoTopAlt : public BasicNode {
public:

private:

};

class PatternWithoutRange : public BasicNode {
public:

private:

};

class LiteralPattern : public BasicNode {
public:

private:

};

class IdentifierPattern : public BasicNode {
public:

private:

};

class WildcardPattern : public BasicNode {
public:

private:

};

class RestPattern : public BasicNode {
public:

private:

};

class ReferencePattern : public BasicNode {
public:

private:

};

class StructPattern : public BasicNode {
public:

private:

};

class StructPatternElements : public BasicNode {
public:

private:

};

class StructPatternEtCetera : public BasicNode {
public:

private:

};

class StructPatternFields : public BasicNode {
public:

private:

};

class StructPatternField : public BasicNode {
public:

private:

};

class TuplePattern : public BasicNode {
public:

private:

};

class TuplePatternItems : public BasicNode {
public:

private:

};

class GroupedPattern : public BasicNode {
public:

private:

};

class SlicePattern : public BasicNode {
public:

private:

};

class SlicePatternItems : public BasicNode {
public:

private:

};

class PathPattern : public BasicNode {
public:

private:

};


}

#endif // INSOMNIA_AST_H