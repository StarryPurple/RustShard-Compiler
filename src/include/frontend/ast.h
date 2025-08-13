#ifndef INSOMNIA_AST_H
#define INSOMNIA_AST_H

#include <memory>
#include <variant>
#include <vector>

#include "ast_fwd.h"
#include "ast_visitor.h"

namespace insomnia::ast {

class NodeBase {
public:
  virtual ~NodeBase() = default;
  virtual void accept(Visitor &visitor) = 0;
};

class Crate : public NodeBase {
public:
  template <class T>
  Crate(T &&items) : _items(std::forward<T>(items)) {}
  void accept(Visitor &visitor) override;
private:
  std::vector<std::unique_ptr<Item>> _items;
};

class Item : public NodeBase {
public:
  template <class T>
  Item(T &&vis_item) : _vis_item(std::forward<T>(vis_item)) {}
  void accept(Visitor &visitor) override;
private:
  std::unique_ptr<VisItem> _vis_item;
};

class VisItem : public NodeBase {
public:
  template <class T>
  VisItem(T &&spec_item) : _spec_item(std::forward<T>(spec_item)) {}
  void accept(Visitor &visitor) override;
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

class Function : public NodeBase {
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
  void accept(Visitor &visitor) override;
private:
  bool _is_const;
  std::string_view _fn_name;
  std::unique_ptr<FunctionParameters> _params;
  std::unique_ptr<Type> _type;
  std::unique_ptr<BlockExpression> _block_expr;
};

class FunctionParameters : public NodeBase {
public:

private:

};

class FunctionParam : public NodeBase {
public:

private:

};

class FunctionParamPattern : public NodeBase {
public:

private:

};

class SelfParam : public NodeBase {
public:

private:

};

class Type : public NodeBase {
public:

private:

};

class TypeNoBounds : public NodeBase {
public:

private:

};

class ParenthesizedType : public NodeBase {
public:

private:

};

class TupleType : public NodeBase {
public:

private:

};

class ReferenceType : public NodeBase {
public:

private:

};

class ArrayType : public NodeBase {
public:

private:

};

class SliceType : public NodeBase {
public:

private:

};

class Struct : public NodeBase {
public:

private:

};

class StructStruct : public NodeBase {
public:

private:

};

class StructFields : public NodeBase {
public:

private:

};

class StructField : public NodeBase {
public:

private:

};

class Enumeration : public NodeBase {
public:

private:

};

class EnumItems : public NodeBase {
public:

private:

};

class EnumItem : public NodeBase {
public:

private:

};

class EnumItemDiscriminant : public NodeBase {
public:

private:

};

class ConstantItem : public NodeBase {
public:

private:

};

class Trait : public NodeBase {
public:

private:

};

class AssociatedItem : public NodeBase {
public:

private:

};

class Implementation : public NodeBase {
public:

private:

};

class InherentImpl : public NodeBase {
public:

private:

};

class TraitImpl : public NodeBase {
public:

private:

};

class TypePath : public NodeBase {
public:

private:

};

class TypePathSegment : public NodeBase {
public:

private:

};

class PathIdentSegment : public NodeBase {
public:

private:

};

class Expression : public NodeBase {
public:

private:

};

class ExpressionWithoutBlock : public NodeBase {
public:

private:

};

class LiteralExpression : public NodeBase {
public:

private:

};

class PathExpression : public NodeBase {
public:

private:

};

class PathInExpression : public NodeBase {
public:

private:

};

class PathExprSegment : public NodeBase {
public:

private:

};

class OperatorExpression : public NodeBase {
public:

private:

};

class BorrowExpression : public NodeBase {
public:

private:

};

class DereferenceExpression : public NodeBase {
public:

private:

};

class NegationExpression : public NodeBase {
public:

private:

};

class ArithmeticOrLogicalExpression : public NodeBase {
public:

private:

};

class ComparisonExpression : public NodeBase {
public:

private:

};

class LazyBooleanExpression : public NodeBase {
public:

private:

};

class TypeCastExpression : public NodeBase {
public:

private:

};

class AssignmentExpression : public NodeBase {
public:

private:

};

class CompoundAssignmentExpression : public NodeBase {
public:

private:

};

class GroupedExpression : public NodeBase {
public:

private:

};

class ArrayExpression : public NodeBase {
public:

private:

};

class ArrayElements : public NodeBase {
public:

private:

};

class IndexExpression : public NodeBase {
public:

private:

};

class TupleExpression : public NodeBase {
public:

private:

};

class TupleElements : public NodeBase {
public:

private:

};

class TupleIndexingExpression : public NodeBase {
public:

private:

};

class StructExpression : public NodeBase {
public:

private:

};

class StructExprFields : public NodeBase {
public:

private:

};

class StructExprField : public NodeBase {
public:

private:

};

class StructBase : public NodeBase {
public:

private:

};

class CallExpression : public NodeBase {
public:

private:

};

class CallParams : public NodeBase {
public:

private:

};

class MethodCallExpression : public NodeBase {
public:

private:

};

class FieldExpression : public NodeBase {
public:

private:

};

class ContinueExpression : public NodeBase {
public:

private:

};

class BreakExpression : public NodeBase {
public:

private:

};

class RangeExpression : public NodeBase {
public:

private:

};

class RangeExpr : public NodeBase {
public:

private:

};

class RangFromExpr : public NodeBase {
public:

private:

};

class RangeToExpr : public NodeBase {
public:

private:

};

class RangeFullExpr : public NodeBase {
public:

private:

};

class RangeInclusiveExpr : public NodeBase {
public:

private:

};

class RangeToInclusiveExpr : public NodeBase {
public:

private:

};

class ReturnExpression : public NodeBase {
public:

private:

};

class UnderscoreExpression : public NodeBase {
public:

private:

};

class ExpressionWithBlock : public NodeBase {
public:

private:

};

class BlockExpression : public NodeBase {
public:

private:

};

class Statements : public NodeBase {
public:

private:

};

class Statement : public NodeBase {
public:

private:

};

class LetStatement : public NodeBase {
public:

private:

};

class ExpressionStatement : public NodeBase {
public:

private:

};

class LoopExpression : public NodeBase {
public:

private:

};

class InfiniteLoopExpression : public NodeBase {
public:

private:

};

class PredicateLoopExpression : public NodeBase {
public:

private:

};

class IfExpression : public NodeBase {
public:

private:

};

class Conditions : public NodeBase {
public:

private:

};

class MatchExpression : public NodeBase {
public:

private:

};

class MatchArms : public NodeBase {
public:

private:

};

class MatchArm : public NodeBase {
public:

private:

};

class MatchArmGuard : public NodeBase {
public:

private:

};

class Pattern : public NodeBase {
public:

private:

};

class PatternNoTopAlt : public NodeBase {
public:

private:

};

class PatternWithoutRange : public NodeBase {
public:

private:

};

class LiteralPattern : public NodeBase {
public:

private:

};

class IdentifierPattern : public NodeBase {
public:

private:

};

class WildcardPattern : public NodeBase {
public:

private:

};

class RestPattern : public NodeBase {
public:

private:

};

class ReferencePatter : public NodeBase {
public:

private:

};

class StructPattern : public NodeBase {
public:

private:

};

class StructPatternElements : public NodeBase {
public:

private:

};

class StructPatternEtCetera : public NodeBase {
public:

private:

};

class StructPatternFields : public NodeBase {
public:

private:

};

class StructPatternField : public NodeBase {
public:

private:

};

class TuplePattern : public NodeBase {
public:

private:

};

class TuplePatternItems : public NodeBase {
public:

private:

};

class GroupedPattern : public NodeBase {
public:

private:

};

class SlicePattern : public NodeBase {
public:

private:

};

class SlicePatternItems : public NodeBase {
public:

private:

};

class PathPattern : public NodeBase {
public:

private:

};


}

#endif // INSOMNIA_AST_H