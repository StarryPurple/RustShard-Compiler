#include "frontend/ast.h"

namespace insomnia::ast {

void Crate::accept(Visitor &visitor) {
  visitor.visit(*this);
  for(auto &item : _items)
    item->accept(visitor);
}

void Item::accept(Visitor &visitor) {
  visitor.visit(*this);
  _vis_item->accept(visitor);
}

void VisItem::accept(Visitor &visitor) {
  visitor.visit(*this);
  std::visit([&](auto &item) { item->accept(visitor); }, _spec_item);
}

void Function::accept(Visitor &visitor) {
  visitor.visit(*this);
  if(_params) _params->accept(visitor);
  if(_type) _type->accept(visitor);
  if(_block_expr) _block_expr->accept(visitor);
}


}