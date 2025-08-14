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
  _expr->accept(visitor);
  visitor.post_visit(*this);
}

void SliceType::accept(BasicVisitor &visitor) {
  visitor.pre_visit(*this);
  _type->accept(visitor);
  visitor.post_visit(*this);
}


}