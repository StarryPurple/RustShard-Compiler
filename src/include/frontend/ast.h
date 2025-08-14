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

class BaseNode {
public:
  virtual ~BaseNode() = default;
  virtual void accept(BaseVisitor &visitor) = 0;
};

class Crate : public BaseNode {
public:
  template <class T>
  explicit Crate(T &&items) : _items(std::forward<T>(items)) {}
  void accept(BaseVisitor &visitor) override;
private:
  std::vector<std::unique_ptr<Item>> _items;
};

class Item : public BaseNode {
public:
  template <class T>
  explicit Item(T &&vis_item) : _vis_item(std::forward<T>(vis_item)) {}
  void accept(BaseVisitor &visitor) override;
private:
  std::unique_ptr<VisItem> _vis_item;
};

class VisItem : public BaseNode {
public:
  template <class T>
  explicit VisItem(T &&spec_item) : _spec_item(std::forward<T>(spec_item)) {}
  void accept(BaseVisitor &visitor) override;
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

class Function : public BaseNode {
public:
  Function(
    bool is_const,
    std::string_view fn_name,
    std::unique_ptr<FunctionParameters> params,
    std::unique_ptr<Type> type,
    std::unique_ptr<BlockExpression> block_expr
    ) :
  _is_const(is_const), _fn_name(fn_name), _params(std::move(params)),
  _type(std::move(type)), _block_expr(std::move(block_expr)) {}
  void accept(BaseVisitor &visitor) override;
private:
  bool _is_const;
  std::string_view _fn_name;
  std::unique_ptr<FunctionParameters> _params;
  std::unique_ptr<Type> _type;
  std::unique_ptr<BlockExpression> _block_expr;
};

class FunctionParameters : public BaseNode {
public:
  template <class T>
  FunctionParameters(
    std::unique_ptr<SelfParam> self_param,
    T &&func_params
  ) :
  _self_param(std::move(self_param)), _func_params(std::forward<T>(func_params)) {}
  void accept(BaseVisitor &visitor) override;
private:
  std::unique_ptr<SelfParam> _self_param;
  std::vector<std::unique_ptr<FunctionParam>> _func_params;
};

class FunctionParam : public BaseNode {
public:
  template <class T>
  FunctionParam(T &&spec) : _spec(std::forward<T>(spec)) {}
  void accept(BaseVisitor &visitor) override;
private:
  std::variant<
    std::unique_ptr<FunctionParamPattern>,
    std::unique_ptr<Type>
  > _spec;
};

class FunctionParamPattern : public BaseNode {
public:
  FunctionParamPattern(
    std::unique_ptr<PatternNoTopAlt> pattern,
    std::unique_ptr<Type> type
  ) :
  _pattern(std::move(pattern)), _type(std::move(type)) {}
  void accept(BaseVisitor &visitor) override;
private:
  std::unique_ptr<PatternNoTopAlt> _pattern;
  std::unique_ptr<Type> _type;
};

class SelfParam : public BaseNode {
public:
  SelfParam(
    bool is_ref, bool is_mut,
    std::unique_ptr<Type> type
  ) :
  _is_ref(is_ref), _is_mut(is_mut), _type(std::move(type)) {}
  void accept(BaseVisitor &visitor) override;
private:
  bool _is_ref;
  bool _is_mut;
  std::unique_ptr<Type> _type;
};

class Type : public BaseNode {
public:

private:

};

class TypeNoBounds : public BaseNode {
public:

private:

};

class ParenthesizedType : public BaseNode {
public:

private:

};

class TupleType : public BaseNode {
public:

private:

};

class ReferenceType : public BaseNode {
public:

private:

};

class ArrayType : public BaseNode {
public:

private:

};

class SliceType : public BaseNode {
public:

private:

};

class Struct : public BaseNode {
public:

private:

};

class StructStruct : public BaseNode {
public:

private:

};

class StructFields : public BaseNode {
public:

private:

};

class StructField : public BaseNode {
public:

private:

};

class Enumeration : public BaseNode {
public:

private:

};

class EnumItems : public BaseNode {
public:

private:

};

class EnumItem : public BaseNode {
public:

private:

};

class EnumItemDiscriminant : public BaseNode {
public:

private:

};

class ConstantItem : public BaseNode {
public:

private:

};

class Trait : public BaseNode {
public:

private:

};

class AssociatedItem : public BaseNode {
public:

private:

};

class Implementation : public BaseNode {
public:

private:

};

class InherentImpl : public BaseNode {
public:

private:

};

class TraitImpl : public BaseNode {
public:

private:

};

class TypePath : public BaseNode {
public:

private:

};

class TypePathSegment : public BaseNode {
public:

private:

};

class PathIdentSegment : public BaseNode {
public:

private:

};

class Expression : public BaseNode {
public:

private:

};

class ExpressionWithoutBlock : public BaseNode {
public:

private:

};

class LiteralExpression : public BaseNode {
public:

private:

};

class PathExpression : public BaseNode {
public:

private:

};

class PathInExpression : public BaseNode {
public:

private:

};

class PathExprSegment : public BaseNode {
public:

private:

};

class OperatorExpression : public BaseNode {
public:

private:

};

class BorrowExpression : public BaseNode {
public:

private:

};

class DereferenceExpression : public BaseNode {
public:

private:

};

class NegationExpression : public BaseNode {
public:

private:

};

class ArithmeticOrLogicalExpression : public BaseNode {
public:

private:

};

class ComparisonExpression : public BaseNode {
public:

private:

};

class LazyBooleanExpression : public BaseNode {
public:

private:

};

class TypeCastExpression : public BaseNode {
public:

private:

};

class AssignmentExpression : public BaseNode {
public:

private:

};

class CompoundAssignmentExpression : public BaseNode {
public:

private:

};

class GroupedExpression : public BaseNode {
public:

private:

};

class ArrayExpression : public BaseNode {
public:

private:

};

class ArrayElements : public BaseNode {
public:

private:

};

class IndexExpression : public BaseNode {
public:

private:

};

class TupleExpression : public BaseNode {
public:

private:

};

class TupleElements : public BaseNode {
public:

private:

};

class TupleIndexingExpression : public BaseNode {
public:

private:

};

class StructExpression : public BaseNode {
public:

private:

};

class StructExprFields : public BaseNode {
public:

private:

};

class StructExprField : public BaseNode {
public:

private:

};

class StructBase : public BaseNode {
public:

private:

};

class CallExpression : public BaseNode {
public:

private:

};

class CallParams : public BaseNode {
public:

private:

};

class MethodCallExpression : public BaseNode {
public:

private:

};

class FieldExpression : public BaseNode {
public:

private:

};

class ContinueExpression : public BaseNode {
public:

private:

};

class BreakExpression : public BaseNode {
public:

private:

};

class RangeExpression : public BaseNode {
public:

private:

};

class RangeExpr : public BaseNode {
public:

private:

};

class RangFromExpr : public BaseNode {
public:

private:

};

class RangeToExpr : public BaseNode {
public:

private:

};

class RangeFullExpr : public BaseNode {
public:

private:

};

class RangeInclusiveExpr : public BaseNode {
public:

private:

};

class RangeToInclusiveExpr : public BaseNode {
public:

private:

};

class ReturnExpression : public BaseNode {
public:

private:

};

class UnderscoreExpression : public BaseNode {
public:

private:

};

class ExpressionWithBlock : public BaseNode {
public:

private:

};

class BlockExpression : public BaseNode {
public:

private:

};

class Statements : public BaseNode {
public:

private:

};

class Statement : public BaseNode {
public:

private:

};

class LetStatement : public BaseNode {
public:

private:

};

class ExpressionStatement : public BaseNode {
public:

private:

};

class LoopExpression : public BaseNode {
public:

private:

};

class InfiniteLoopExpression : public BaseNode {
public:

private:

};

class PredicateLoopExpression : public BaseNode {
public:

private:

};

class IfExpression : public BaseNode {
public:

private:

};

class Conditions : public BaseNode {
public:

private:

};

class MatchExpression : public BaseNode {
public:

private:

};

class MatchArms : public BaseNode {
public:

private:

};

class MatchArm : public BaseNode {
public:

private:

};

class MatchArmGuard : public BaseNode {
public:

private:

};

class Pattern : public BaseNode {
public:

private:

};

class PatternNoTopAlt : public BaseNode {
public:

private:

};

class PatternWithoutRange : public BaseNode {
public:

private:

};

class LiteralPattern : public BaseNode {
public:

private:

};

class IdentifierPattern : public BaseNode {
public:

private:

};

class WildcardPattern : public BaseNode {
public:

private:

};

class RestPattern : public BaseNode {
public:

private:

};

class ReferencePattern : public BaseNode {
public:

private:

};

class StructPattern : public BaseNode {
public:

private:

};

class StructPatternElements : public BaseNode {
public:

private:

};

class StructPatternEtCetera : public BaseNode {
public:

private:

};

class StructPatternFields : public BaseNode {
public:

private:

};

class StructPatternField : public BaseNode {
public:

private:

};

class TuplePattern : public BaseNode {
public:

private:

};

class TuplePatternItems : public BaseNode {
public:

private:

};

class GroupedPattern : public BaseNode {
public:

private:

};

class SlicePattern : public BaseNode {
public:

private:

};

class SlicePatternItems : public BaseNode {
public:

private:

};

class PathPattern : public BaseNode {
public:

private:

};


}

#endif // INSOMNIA_AST_H