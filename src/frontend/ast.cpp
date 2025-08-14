#include "frontend/ast.h"

namespace insomnia::ast {

void Crate::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  for(auto &item : _items) item->accept(visitor);
  visitor.post_visit(*this);
}

void Item::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  _vis_item->accept(visitor);
  visitor.post_visit(*this);
}

void VisItem::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  std::visit([&](auto &item) { item->accept(visitor); }, _spec_item);
  visitor.post_visit(*this);
}

void Function::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_params) _params->accept(visitor);
  if(_type) _type->accept(visitor);
  if(_block_expr) _block_expr->accept(visitor);
  visitor.post_visit(*this);
}

void FunctionParameters::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_self_param) _self_param->accept(visitor);
  for(auto &param : _func_params) param->accept(visitor);
  visitor.post_visit(*this);
}

void FunctionParam::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  std::visit([&](auto &spec) { spec->accept(visitor); }, _spec);
  visitor.post_visit(*this);
}

void FunctionParamPattern::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  _pattern->accept(visitor);
  _type->accept(visitor);
  visitor.post_visit(*this);
}

void SelfParam::accept(BaseVisitor &visitor) {
  visitor.pre_visit(*this);
  if(_type) _type->accept(visitor);
  visitor.post_visit(*this);
}


}