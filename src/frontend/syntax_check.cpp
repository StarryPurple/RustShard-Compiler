#include "syntax_check.h"

#include <limits>

namespace insomnia::rust_shard::ast {
void ConstEvaluator::postVisit(LiteralExpression &node) {
  using sem_type::PrimitiveType;
  auto prime = node.prime();
  sem_type::TypePtr type = _type_pool->make_type<sem_type::PrimitiveType>(prime);
  std::visit([&](auto &&arg) {
    sem_const::ConstPrimitive primitive_val(arg);
    node.set_const_value(
      _const_pool->make_const<sem_const::ConstPrimitive>(type, primitive_val)
    );
  }, node.spec_value());
}

const std::string ConstEvaluator::kErrTag = "ConstEvaluator Failure";

void ConstEvaluator::postVisit(BorrowExpression &node) {
  if(!node.expr()->const_value().has_value()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = *node.expr()->const_value();
  node.set_const_value(_const_pool->make_const<sem_const::ConstReference>(
    _type_pool->make_type<sem_type::ReferenceType>(inner->type()),
    inner
  ));
}

void ConstEvaluator::postVisit(DereferenceExpression &node) {
  if(!node.expr()->const_value().has_value()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = *node.expr()->const_value();
  if(auto ref = inner->get_if<sem_const::ConstReference>()) {
    node.set_const_value(ref->ref);
  } else {
    _recorder->tagged_report(kErrTag, "Dereferencing a not-borrowed type");
  }
}

void ConstEvaluator::postVisit(NegationExpression &node) {
  if(!node.expr()->const_value().has_value()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = *node.expr()->const_value();
  auto primitive = inner->get_if<sem_const::ConstPrimitive>();
  if(!primitive) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in negation expression");
    return;
  }
  std::visit([&]<typename T0>(T0 &&arg){
    using T = std::decay_t<T0>;
    if constexpr(type_utils::is_one_of<T, std::int64_t, float, double>) {
      if constexpr(std::is_same_v<T, std::int64_t>) {
        using sem_type::TypePrime;
        auto prime = inner->type().get<sem_type::PrimitiveType>()->prime();
        if(
          (prime == TypePrime::kI8 && arg == std::numeric_limits<std::int8_t>::min()) ||
          (prime == TypePrime::kI16 && arg == std::numeric_limits<std::int16_t>::min()) ||
          (prime == TypePrime::kI32 && arg == std::numeric_limits<std::int32_t>::min()) ||
          (prime == TypePrime::kI64 && arg == std::numeric_limits<std::int64_t>::min()) ||
          (prime == TypePrime::kISize && arg == std::numeric_limits<std::intptr_t>::min())
        ) {
          _recorder->tagged_report(kErrTag, "Integer overflow in negation expression");
          return;
        }
      }
      node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
        inner->type(), // still primitive
        -arg // negation
      ));
    } else if constexpr(std::is_same_v<T, bool>) {
      node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
        inner->type(), // still primitive
        !arg // logic not
      ));
    } else {
      _recorder->tagged_report(kErrTag, "Inner primitive type not supported in negation expression");
    }
  }, primitive->value);
}

void ConstEvaluator::postVisit(ArithmeticOrLogicalExpression &node) {
  if(!node.expr1()->const_value().has_value()) {
    _recorder->tagged_report(kErrTag, "Inner expr(lhs) const evaluation failed");
    return;
  }
  if(!node.expr2()->const_value().has_value()) {
    _recorder->tagged_report(kErrTag, "Inner expr(rhs) const evaluation failed");
    return;
  }
  auto inner1 = *node.expr1()->const_value();
  auto inner2 = *node.expr2()->const_value();
  auto prime1 = inner1->get_if<sem_const::ConstPrimitive>();
  auto prime2 = inner2->get_if<sem_const::ConstPrimitive>();
  if(!prime1) {
    _recorder->tagged_report(kErrTag, "Inner type(lhs) not primitive in arithmetic/logical expression");
    return;
  }
  if(!prime2) {
    _recorder->tagged_report(kErrTag, "Inner type(lhs) not primitive in arithmetic/logical expression");
    return;
  }

}

void ConstEvaluator::postVisit(ComparisonExpression &node) {

}

void ConstEvaluator::postVisit(LazyBooleanExpression &node) {

}

void ConstEvaluator::postVisit(TypeCastExpression &node) {

}

void ConstEvaluator::postVisit(AssignmentExpression &node) {

}

void ConstEvaluator::postVisit(CompoundAssignmentExpression &node) {

}

void ConstEvaluator::postVisit(GroupedExpression &node) {

}

void ConstEvaluator::postVisit(ArrayExpression &node) {

}

void ConstEvaluator::postVisit(IndexExpression &node) {

}

void ConstEvaluator::postVisit(TupleExpression &node) {

}

void ConstEvaluator::postVisit(TupleIndexingExpression &node) {

}

void ConstEvaluator::postVisit(StructExpression &node) {

}

void ConstEvaluator::postVisit(FieldExpression &node) {

}

void ConstEvaluator::postVisit(ContinueExpression &node) {

}

void ConstEvaluator::postVisit(BreakExpression &node) {

}




}
