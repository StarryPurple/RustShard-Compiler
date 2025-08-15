#include "frontend/ast.h"

namespace insomnia::ast {

void Crate::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  for(auto &item : _items) item->accept(visitor);
  visitor.post_visit(*this);
}

void Item::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _vis_item->accept(visitor);
  visitor.post_visit(*this);
}

void VisItem::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  std::visit([&](auto &item) { item->accept(visitor); }, _spec_item);
  visitor.post_visit(*this);
}

void Function::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_params_opt) _params_opt->accept(visitor);
  if(_res_type_opt) _res_type_opt->accept(visitor);
  if(_block_expr_opt) _block_expr_opt->accept(visitor);
  visitor.post_visit(*this);
}

void FunctionParameters::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_self_param_opt) _self_param_opt->accept(visitor);
  for(auto &param : _func_params) param->accept(visitor);
  visitor.post_visit(*this);
}

void FunctionParam::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  std::visit([&](auto &spec) { spec->accept(visitor); }, _spec);
  visitor.post_visit(*this);
}

void FunctionParamPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _pattern->accept(visitor);
  _type->accept(visitor);
  visitor.post_visit(*this);
}

void SelfParam::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_type) _type->accept(visitor);
  visitor.post_visit(*this);
}

void Type::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type_no_bounds->accept(visitor);
  visitor.post_visit(*this);
}

void TypeNoBounds::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  std::visit([&](auto &spec) { spec->accept(visitor); }, _spec);
  visitor.post_visit(*this);
}

void ParenthesizedType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type->accept(visitor);
  visitor.post_visit(*this);
}

void TupleType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  for(auto &type: _types) type->accept(visitor);
  visitor.post_visit(*this);
}

void ReferenceType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type_no_bounds->accept(visitor);
  visitor.post_visit(*this);
}

void ArrayType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type->accept(visitor);
  _const_expr->accept(visitor);
  visitor.post_visit(*this);
}

void SliceType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type->accept(visitor);
  visitor.post_visit(*this);
}

void Struct::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _ss->accept(visitor);
  visitor.post_visit(*this);
}

void StructStruct::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_fields_opt) _fields_opt->accept(visitor);
  visitor.post_visit(*this);
}

void StructFields::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  for(auto &field: _fields) field->accept(visitor);
  visitor.post_visit(*this);
}

void StructField::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type->accept(visitor);
  visitor.post_visit(*this);
}

void Enumeration::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_items_opt) _items_opt->accept(visitor);
  visitor.post_visit(*this);
}

void EnumItems::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  for(auto &item: _items) item->accept(visitor);
  visitor.post_visit(*this);
}

void EnumItem::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_discr_opt) _discr_opt->accept(visitor);
  visitor.post_visit(*this);
}

void EnumItemDiscriminant::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _const_expr->accept(visitor);
  visitor.post_visit(*this);
}

void ConstantItem::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Trait::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void AssociatedItem::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Implementation::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void InherentImpl::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TraitImpl::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TypePath::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TypePathSegment::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PathIdentSegment::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Expression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ExpressionWithoutBlock::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void LiteralExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PathExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PathInExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PathExprSegment::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void OperatorExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void BorrowExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void DereferenceExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void NegationExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ArithmeticOrLogicalExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ComparisonExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void LazyBooleanExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TypeCastExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void AssignmentExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void CompoundAssignmentExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void GroupedExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ArrayExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ArrayElements::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void IndexExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TupleExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TupleElements::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TupleIndexingExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructExprFields::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructExprField::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructBase::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void CallExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void CallParams::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void MethodCallExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void FieldExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ContinueExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void BreakExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangFromExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeToExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeFullExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeInclusiveExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RangeToInclusiveExpr::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ReturnExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void UnderscoreExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ExpressionWithBlock::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void BlockExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Statements::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Statement::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void LetStatement::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ExpressionStatement::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void LoopExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void InfiniteLoopExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PredicateLoopExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void IfExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Conditions::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void MatchExpression::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void MatchArms::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void MatchArm::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void MatchArmGuard::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void Pattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PatternNoTopAlt::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PatternWithoutRange::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void LiteralPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void IdentifierPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void WildcardPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void RestPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void ReferencePattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructPatternElements::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructPatternEtCetera::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructPatternFields::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void StructPatternField::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TuplePattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void TuplePatternItems::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void GroupedPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void SlicePattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void SlicePatternItems::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}

void PathPattern::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  visitor.post_visit(*this);
}



}