#include "recursive_visitor.h"

#include "ast.h"

namespace insomnia::rust_shard::ast {

void RecursiveVisitor::traverse(Crate &crate) {
  crate.accept(*this);
}

void RecursiveVisitor::visit(Crate &node) {
  preVisit(node);
  for(const auto &item: node.items())
    item->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Item &node) {
  preVisit(node);
  node.vis_item()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Function &node) {
  preVisit(node);
  if(node.params_opt()) node.params_opt()->accept(*this);
  if(node.res_type_opt()) node.res_type_opt()->accept(*this);
  if(node.body_opt()) node.body_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(FunctionBodyExpr &node) {
  preVisit(node);
  if(node.stmts_opt()) node.stmts_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(FunctionParameters &node) {
  preVisit(node);
  if(node.self_param_opt()) node.self_param_opt()->accept(*this);
  for(const auto &func_param: node.func_params())
    func_param->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(FunctionParamPattern &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  node.type()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(FunctionParamType &node) {
  preVisit(node);
  node.type()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(SelfParam &node) {
  preVisit(node);
  if(node.type_opt()) node.type_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ParenthesizedType &node) {
  preVisit(node);
  node.type()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TupleType &node) {
  preVisit(node);
  for(const auto &type: node.types())
    type->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ReferenceType &node) {
  preVisit(node);
  node.type_no_bounds()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ArrayType &node) {
  preVisit(node);
  node.type()->accept(*this);
  node.const_expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(SliceType &node) {
  preVisit(node);
  node.type()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructStruct &node) {
  preVisit(node);
  if(node.fields_opt()) node.fields_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructFields &node) {
  preVisit(node);
  for(const auto &field: node.fields())
    field->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructField &node) {
  preVisit(node);
  node.type()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Enumeration &node) {
  preVisit(node);
  if(node.items_opt()) node.items_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(EnumItems &node) {
  preVisit(node);
  for(const auto &item: node.items())
    item->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(EnumItem &node) {
  preVisit(node);
  if(node.discr_opt()) node.discr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(EnumItemDiscriminant &node) {
  preVisit(node);
  node.const_expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ConstantItem &node) {
  preVisit(node);
  node.type()->accept(*this);
  if(node.const_expr_opt()) node.const_expr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Trait &node) {
  preVisit(node);
  for(const auto &asso_item: node.asso_items())
    asso_item->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(AssociatedTypeAlias &node) {
  preVisit(node);
  node.alias()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(AssociatedConstantItem &node) {
  preVisit(node);
  node.citem()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(AssociatedFunction &node) {
  preVisit(node);
  node.func()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TypeAlias &node) {
  preVisit(node);
  if(node.type_opt()) node.type_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(InherentImpl &node) {
  preVisit(node);
  node.type()->accept(*this);
  for(const auto &asso_item: node.asso_items())
    asso_item->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TraitImpl &node) {
  preVisit(node);
  node.type_path()->accept(*this);
  node.tar_type()->accept(*this);
  for(const auto &asso_item: node.asso_items())
    asso_item->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TypePath &node) {
  preVisit(node);
  for(const auto &segment: node.segments())
    segment->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TypePathSegment &node) {
  preVisit(node);
  node.ident_segment()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(PathIdentSegment &node) {
  preVisit(node);
  // Nothing to traverse here.
  postVisit(node);
}

void RecursiveVisitor::visit(LiteralExpression &node) {
  preVisit(node);
  // Nothing to traverse here.
  postVisit(node);
}

void RecursiveVisitor::visit(PathInExpression &node) {
  preVisit(node);
  for(const auto &segment: node.segments())
    segment->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(PathExprSegment &node) {
  preVisit(node);
  node.ident_seg()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(BorrowExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(DereferenceExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(NegationExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ArithmeticOrLogicalExpression &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ComparisonExpression &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(LazyBooleanExpression &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TypeCastExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  node.type_no_bounds()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(AssignmentExpression &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(CompoundAssignmentExpression &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(GroupedExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ArrayExpression &node) {
  preVisit(node);
  if(node.elements_opt()) node.elements_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ExplicitArrayElements &node) {
  preVisit(node);
  for(const auto &expr: node.expr_list())
    expr->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RepeatedArrayElements &node) {
  preVisit(node);
  node.val_expr()->accept(*this);
  node.len_expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(IndexExpression &node) {
  preVisit(node);
  node.expr_obj()->accept(*this);
  node.expr_index()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TupleExpression &node) {
  preVisit(node);
  if(node.elems_opt()) node.elems_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TupleElements &node) {
  preVisit(node);
  for(const auto &expr: node.expr_list())
    expr->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TupleIndexingExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructExpression &node) {
  preVisit(node);
  node.path()->accept(*this);
  if(node.fields_opt()) node.fields_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructExprFields &node) {
  preVisit(node);
  for(const auto &field: node.fields())
    field->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(NamedStructExprField &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(IndexStructExprField &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(CallExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  if(node.params_opt()) node.params_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(CallParams &node) {
  preVisit(node);
  for(const auto &expr: node.expr_list())
    expr->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(MethodCallExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  node.segment()->accept(*this);
  if(node.params_opt()) node.params_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(FieldExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ContinueExpression &node) {
  preVisit(node);
  // Nothing here to traverse.
  postVisit(node);
}

void RecursiveVisitor::visit(BreakExpression &node) {
  preVisit(node);
  if(node.expr_opt()) node.expr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RangeExpr &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RangeFromExpr &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RangeToExpr &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RangeFullExpr &node) {
  preVisit(node);
  // Nothing here to traverse.
  postVisit(node);
}

void RecursiveVisitor::visit(RangeInclusiveExpr &node) {
  preVisit(node);
  node.expr1()->accept(*this);
  node.expr2()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(RangeToInclusiveExpr &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ReturnExpression &node) {
  preVisit(node);
  if(node.expr_opt()) node.expr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(UnderscoreExpression &node) {
  preVisit(node);
  // Nothing here to traverse.
  postVisit(node);
}

void RecursiveVisitor::visit(BlockExpression &node) {
  preVisit(node);
  if(node.stmts_opt()) node.stmts_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Statements &node) {
  preVisit(node);
  for(const auto &stmt: node.stmts())
    stmt->accept(*this);
  if(node.expr_opt()) node.expr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(EmptyStatement &node) {
  preVisit(node);
  // Nothing here to traverse.
  postVisit(node);
}

void RecursiveVisitor::visit(ItemStatement &node) {
  preVisit(node);
  node.item()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(LetStatement &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  if(node.type_opt()) node.type_opt()->accept(*this);
  if(node.expr_opt()) node.expr_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(ExpressionStatement &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(InfiniteLoopExpression &node) {
  preVisit(node);
  node.block_expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(PredicateLoopExpression &node) {
  preVisit(node);
  node.cond()->accept(*this);
  node.block_expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(IfExpression &node) {
  preVisit(node);
  node.cond()->accept(*this);
  // ...
  // if(node.block_expr()) node.block_expr()->accept(*this);
  node.block_expr()->accept(*this);
  std::visit([&]<class T0>(const T0& arg) {
    using T = std::decay_t<T0>; // ...
    if constexpr(std::is_same_v<T, std::monostate>) {
      // Nothing here to traverse
    } else if constexpr(std::is_same_v<T, std::unique_ptr<BlockExpression>>) {
      arg->accept(*this);
    } else if constexpr(std::is_same_v<T, std::unique_ptr<IfExpression>>) {
      arg->accept(*this);
    }
  }, node.else_spec());
  postVisit(node);
}

void RecursiveVisitor::visit(Conditions &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(MatchExpression &node) {
  preVisit(node);
  node.expr()->accept(*this);
  if(node.match_arms_opt()) node.match_arms_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(MatchArms &node) {
  preVisit(node);
  for(const auto &arm: node.arms()) {
    arm.first->accept(*this);
    arm.second->accept(*this);
  }
  postVisit(node);
}

void RecursiveVisitor::visit(MatchArm &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  if(node.guard_opt()) node.guard_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(MatchArmGuard &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(Pattern &node) {
  preVisit(node);
  for(const auto &pattern: node.patterns())
    pattern->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(LiteralPattern &node) {
  preVisit(node);
  node.expr()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(IdentifierPattern &node) {
  preVisit(node);
  if(node.pattern_opt()) node.pattern_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(WildcardPattern &node) {
  preVisit(node);
  // Nothing here to traverse.
  postVisit(node);
}

void RecursiveVisitor::visit(ReferencePattern &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructPattern &node) {
  preVisit(node);
  node.path_in_expr()->accept(*this);
  if(node.elems_opt()) node.elems_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructPatternElements &node) {
  preVisit(node);
  node.fields()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructPatternFields &node) {
  preVisit(node);
  for(const auto &field: node.fields())
    field->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(StructPatternField &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TuplePattern &node) {
  preVisit(node);
  if(node.items_opt()) node.items_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(TuplePatternItems &node) {
  preVisit(node);
  for(const auto &pattern: node.patterns())
    pattern->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(GroupedPattern &node) {
  preVisit(node);
  node.pattern()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(SlicePattern &node) {
  preVisit(node);
  if(node.items_opt()) node.items_opt()->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(SlicePatternItems &node) {
  preVisit(node);
  for(const auto &pattern: node.patterns())
    pattern->accept(*this);
  postVisit(node);
}

void RecursiveVisitor::visit(PathPattern &node) {
  preVisit(node);
  node.path_expr()->accept(*this);
  postVisit(node);
}

}