#include "ast_recursive_visitor.h"

#include "ast.h"

namespace insomnia::rust_shard::ast {

void RecursiveVisitor::visit(Crate &node) {
  node.accept()
}
void RecursiveVisitor::visit(Item &node) {}
void RecursiveVisitor::visit(Function &node) {}
void RecursiveVisitor::visit(FunctionParameters &node) {}
void RecursiveVisitor::visit(FunctionParamPattern &node) {}
void RecursiveVisitor::visit(FunctionParamType &node) {}
void RecursiveVisitor::visit(SelfParam &node) {}
void RecursiveVisitor::visit(ParenthesizedType &node) {}
void RecursiveVisitor::visit(TupleType &node) {}
void RecursiveVisitor::visit(ReferenceType &node) {}
void RecursiveVisitor::visit(ArrayType &node) {}
void RecursiveVisitor::visit(SliceType &node) {}
void RecursiveVisitor::visit(StructStruct &node) {}
void RecursiveVisitor::visit(StructFields &node) {}
void RecursiveVisitor::visit(StructField &node) {}
void RecursiveVisitor::visit(Enumeration &node) {}
void RecursiveVisitor::visit(EnumItems &node) {}
void RecursiveVisitor::visit(EnumItem &node) {}
void RecursiveVisitor::visit(EnumItemDiscriminant &node) {}
void RecursiveVisitor::visit(ConstantItem &node) {}
void RecursiveVisitor::visit(Trait &node) {}
void RecursiveVisitor::visit(AssociatedTypeAlias &node) {}
void RecursiveVisitor::visit(AssociatedConstantItem &node) {}
void RecursiveVisitor::visit(AssociatedFunction &node) {}
void RecursiveVisitor::visit(TypeAlias &node) {}
void RecursiveVisitor::visit(InherentImpl &node) {}
void RecursiveVisitor::visit(TraitImpl &node) {}
void RecursiveVisitor::visit(TypePath &node) {}
void RecursiveVisitor::visit(TypePathSegment &node) {}
void RecursiveVisitor::visit(PathIdentSegment &node) {}
void RecursiveVisitor::visit(LiteralExpression &node) {}
void RecursiveVisitor::visit(PathInExpression &node) {}
void RecursiveVisitor::visit(PathExprSegment &node) {}
void RecursiveVisitor::visit(BorrowExpression &node) {}
void RecursiveVisitor::visit(DereferenceExpression &node) {}
void RecursiveVisitor::visit(NegationExpression &node) {}
void RecursiveVisitor::visit(ArithmeticOrLogicalExpression &node) {}
void RecursiveVisitor::visit(ComparisonExpression &node) {}
void RecursiveVisitor::visit(LazyBooleanExpression &node) {}
void RecursiveVisitor::visit(TypeCastExpression &node) {}
void RecursiveVisitor::visit(AssignmentExpression &node) {}
void RecursiveVisitor::visit(CompoundAssignmentExpression &node) {}
void RecursiveVisitor::visit(GroupedExpression &node) {}
void RecursiveVisitor::visit(ArrayExpression &node) {}
void RecursiveVisitor::visit(ExplicitArrayElements &node) {}
void RecursiveVisitor::visit(RepeatedArrayElements &node) {}
void RecursiveVisitor::visit(IndexExpression &node) {}
void RecursiveVisitor::visit(TupleExpression &node) {}
void RecursiveVisitor::visit(TupleElements &node) {}
void RecursiveVisitor::visit(TupleIndexingExpression &node) {}
void RecursiveVisitor::visit(StructExpression &node) {}
void RecursiveVisitor::visit(StructExprFields &node) {}
void RecursiveVisitor::visit(NamedStructExprField &node) {}
void RecursiveVisitor::visit(IndexStructExprField &node) {}
void RecursiveVisitor::visit(CallExpression &node) {}
void RecursiveVisitor::visit(CallParams &node) {}
void RecursiveVisitor::visit(MethodCallExpression &node) {}
void RecursiveVisitor::visit(FieldExpression &node) {}
void RecursiveVisitor::visit(ContinueExpression &node) {}
void RecursiveVisitor::visit(BreakExpression &node) {}
void RecursiveVisitor::visit(RangeExpr &node) {}
void RecursiveVisitor::visit(RangeFromExpr &node) {}
void RecursiveVisitor::visit(RangeToExpr &node) {}
void RecursiveVisitor::visit(RangeFullExpr &node) {}
void RecursiveVisitor::visit(RangeInclusiveExpr &node) {}
void RecursiveVisitor::visit(RangeToInclusiveExpr &node) {}
void RecursiveVisitor::visit(ReturnExpression &node) {}
void RecursiveVisitor::visit(UnderscoreExpression &node) {}
void RecursiveVisitor::visit(BlockExpression &node) {}
void RecursiveVisitor::visit(Statements &node) {}
void RecursiveVisitor::visit(EmptyStatement &node) {}
void RecursiveVisitor::visit(ItemStatement &node) {}
void RecursiveVisitor::visit(LetStatement &node) {}
void RecursiveVisitor::visit(ExpressionStatement &node) {}
void RecursiveVisitor::visit(InfiniteLoopExpression &node) {}
void RecursiveVisitor::visit(PredicateLoopExpression &node) {}
void RecursiveVisitor::visit(IfExpression &node) {}
void RecursiveVisitor::visit(Conditions &node) {}
void RecursiveVisitor::visit(MatchExpression &node) {}
void RecursiveVisitor::visit(MatchArms &node) {}
void RecursiveVisitor::visit(MatchArm &node) {}
void RecursiveVisitor::visit(MatchArmGuard &node) {}
void RecursiveVisitor::visit(Pattern &node) {}
void RecursiveVisitor::visit(LiteralPattern &node) {}
void RecursiveVisitor::visit(IdentifierPattern &node) {}
void RecursiveVisitor::visit(WildcardPattern &node) {}
void RecursiveVisitor::visit(ReferencePattern &node) {}
void RecursiveVisitor::visit(StructPattern &node) {}
void RecursiveVisitor::visit(StructPatternElements &node) {}
void RecursiveVisitor::visit(StructPatternFields &node) {}
void RecursiveVisitor::visit(StructPatternField &node) {}
void RecursiveVisitor::visit(TuplePattern &node) {}
void RecursiveVisitor::visit(TuplePatternItems &node) {}
void RecursiveVisitor::visit(GroupedPattern &node) {}
void RecursiveVisitor::visit(SlicePattern &node) {}
void RecursiveVisitor::visit(SlicePatternItems &node) {}
void RecursiveVisitor::visit(PathPattern &node) {}


}
