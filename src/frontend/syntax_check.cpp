#include "syntax_check.h"

#include <limits>
#include <cmath>

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
  if(!node.expr()->has_constant()) {
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
  if(!node.expr()->has_constant()) {
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
  if(!node.expr()->has_constant()) {
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
      if(node.oper() != Operator::kSub) {
        _recorder->tagged_report(kErrTag, "Invalid operator kSub in negation expression");
        return;
      }
      if constexpr(type_utils::is_one_of<T, std::int64_t>) {
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
      if(node.oper() != Operator::kLogicalNot) {
        _recorder->tagged_report(kErrTag, "Invalid operator kLogicalNot in negation expression");
      }
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
  if (!node.expr1()->has_constant() || !node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner1 = *node.expr1()->const_value();
  auto inner2 = *node.expr2()->const_value();
  auto prime1 = inner1->get_if<sem_const::ConstPrimitive>();
  auto prime2 = inner2->get_if<sem_const::ConstPrimitive>();
  if (!prime1 || !prime2) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in arithmetic/logical expression");
    return;
  }

  auto oper = node.oper();

  // TODO: overflow detection

  if(oper == Operator::kShl || oper == Operator::kShr) {
    std::visit([&]<typename T1, typename T2>(T1 &&arg1, T2 &&arg2) {
      if constexpr(type_utils::is_one_of<T1, std::int64_t, std::uint64_t>) {
        if constexpr(type_utils::is_one_of<T2, std::uint64_t>) {
          if(oper == Operator::kShl) {
            node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
              inner1->type(), arg1 << arg2));
          } else {
            node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
              inner1->type(), arg1 >> arg2));
          }
        } else {
          _recorder->tagged_report(kErrTag, "Shift amount must be an unsigned integer");
        }
      } else {
        _recorder->tagged_report(kErrTag, "Shift operation on non-integer type");
      }
    }, prime1->value, prime2->value);
    return;
  }

  if(inner1->type() != inner2->type()) {
    _recorder->tagged_report(kErrTag, "Mismatched types in arithmetic/logical expression");
    return;
  }

  // TODO: NaN implementation (currently treated as compilation error)

  std::visit([&]<typename T>(T &&arg1, T &&arg2) {
    if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
      if((oper == Operator::kDiv || oper == Operator::kMod) && arg2 == 0) {
        _recorder->tagged_report(kErrTag, "Division by zero");
        return;
      }
      switch(oper) {
      case Operator::kAdd:
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 + arg2)); break;
      case Operator::kSub:
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 - arg2)); break;
      case Operator::kMul:
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 * arg2)); break;
      case Operator::kDiv:
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 / arg2)); break;
      case Operator::kMod:
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          inner1->type(),
          (std::is_floating_point_v<T> ? std::fmod(arg1, arg2) : arg1 % arg2)));
        break;
      case Operator::kBitwiseAnd:
      case Operator::kBitwiseOr:
      case Operator::kBitwiseXor:
        if constexpr(std::is_integral_v<T>) {
          if(oper == Operator::kBitwiseAnd)
            node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 & arg2));
          else if(oper == Operator::kBitwiseOr)
            node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 | arg2));
          else
            node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 ^ arg2));
        } else {
          _recorder->tagged_report(kErrTag, "Bitwise operation on non-integer type");
        }
        break;
      default:
        _recorder->tagged_report(kErrTag, "Invalid operator for arithmetic/logical expression (bitwise)");
        break;
      }
    } else if constexpr(std::is_same_v<T, bool>) {
      if(oper == Operator::kLogicalAnd) {
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 && arg2));
      } else if(oper == Operator::kLogicalOr) {
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 || arg2));
      } else {
        _recorder->tagged_report(kErrTag, "Invalid operator for arithmetic/logical expression (boolean logical)");
      }
    } else {
      _recorder->tagged_report(kErrTag, "Unsupported primitive type in arithmetic/logical expression");
    }
  }, prime1->value, prime2->value);
}

void ConstEvaluator::postVisit(ComparisonExpression &node) {
  if(!node.expr1()->has_constant() || !node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner1 = *node.expr1()->const_value();
  auto inner2 = *node.expr2()->const_value();
  auto prime1 = inner1->get_if<sem_const::ConstPrimitive>();
  auto prime2 = inner2->get_if<sem_const::ConstPrimitive>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in comparison expression");
    return;
  }
  if(inner1->type() != inner2->type()) {
    _recorder->tagged_report(kErrTag, "Mismatched types in comparison expression");
    return;
  }

  std::visit([&]<typename T>(T &&arg1, T &&arg2) {
    bool result = false;
    auto oper = node.oper();

    if constexpr(type_utils::is_one_of<T, char, std::int64_t, std::uint64_t, float, double, bool>) {
      switch(oper) {
      case Operator::kEq: result = (arg1 == arg2); break;
      case Operator::kNe: result = (arg1 != arg2); break;
      case Operator::kGt: result = (arg1 > arg2); break;
      case Operator::kLt: result = (arg1 < arg2); break;
      case Operator::kGe: result = (arg1 >= arg2); break;
      case Operator::kLe: result = (arg1 <= arg2); break;
      default:
        _recorder->tagged_report(kErrTag, "Invalid operator in comparison expression");
        return;
      }
      node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
        _type_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kBool),
        result
      ));
    } else {
      _recorder->tagged_report(kErrTag, "Unsupported primitive type in comparison expression");
    }
  }, prime1->value, prime2->value);
}

void ConstEvaluator::visit(LazyBooleanExpression &node) {
  auto oper = node.oper();
  if(oper != Operator::kLogicalAnd && oper != Operator::kLogicalOr) {
    _recorder->tagged_report(kErrTag, "Unsupported operator in boolean expression");
    return;
  }

  node.expr1()->accept(*this);
  if(!node.expr1()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner1 = *node.expr1()->const_value();
  auto prime1 = inner1->get_if<sem_const::ConstPrimitive>();
  if(!prime1) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in boolean expression");
    return;
  }
  auto result1 = std::get_if<bool>(&prime1->value);
  if(!result1) {
    _recorder->tagged_report(kErrTag, "Inner type not boolean in boolean expression");
    return;
  }
  if(oper == Operator::kLogicalAnd && !*result1) {
    node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
      _type_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kBool),
      false));
    return;
  }
  if(oper == Operator::kLogicalOr && *result1) {
    node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
      _type_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kBool),
      true));
    return;
  }

  node.expr2()->accept(*this);
  if(!node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner2 = *node.expr2()->const_value();
  auto prime2 = inner2->get_if<sem_const::ConstPrimitive>();
  if(!prime2) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in boolean expression");
    return;
  }
  auto result2 = std::get_if<bool>(&prime2->value);
  if(!result2) {
    _recorder->tagged_report(kErrTag, "Inner type not boolean in boolean expression");
    return;
  }

  node.set_const_value(inner2);
}

void ConstEvaluator::postVisit(TypeCastExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = *node.expr()->const_value();
  auto primitive = inner->get_if<sem_const::ConstPrimitive>();
  if(!primitive) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in type cast expression");
    return;
  }

  auto target_type = node.type_no_bounds()->get_type().get_if<sem_type::PrimitiveType>();
  if(!target_type) {
    _recorder->tagged_report(kErrTag, "Target type not primitive in type cast expression");
    return;
  }
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    using sem_type::TypePrime;
    auto target_prime = target_type->prime();
    switch(target_prime) {
    case TypePrime::kBool:
      if constexpr(type_utils::is_one_of<T, bool>) {
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          _type_pool->make_type<sem_type::PrimitiveType>(TypePrime::kBool),
          arg));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast non-boolean to bool");
      }
      break;
    case TypePrime::kI8:
    case TypePrime::kI16:
    case TypePrime::kI32:
    case TypePrime::kI64:
    case TypePrime::kISize: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double, char>) {
        std::int64_t val = 0;
        switch(target_prime) {
        case TypePrime::kI8: val = static_cast<std::int8_t>(arg); break;
        case TypePrime::kI16: val = static_cast<std::int16_t>(arg); break;
        case TypePrime::kI32: val = static_cast<std::int32_t>(arg); break;
        case TypePrime::kI64: val = static_cast<std::int64_t>(arg); break;
        case TypePrime::kISize: val = static_cast<std::intptr_t>(arg); break;
        default: break;
        }
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          _type_pool->make_type<sem_type::PrimitiveType>(target_prime),
          val
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to signed integer");
      }
    } break;

    case TypePrime::kU8:
    case TypePrime::kU16:
    case TypePrime::kU32:
    case TypePrime::kU64:
    case TypePrime::kUSize: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double, char>) {
        std::uint64_t val = 0;
        switch(target_prime) {
        case TypePrime::kU8: val = static_cast<std::uint8_t>(arg); break;
        case TypePrime::kU16: val = static_cast<std::uint16_t>(arg); break;
        case TypePrime::kU32: val = static_cast<std::uint32_t>(arg); break;
        case TypePrime::kU64: val = static_cast<std::uint64_t>(arg); break;
        case TypePrime::kUSize: val = static_cast<std::uintptr_t>(arg); break;
        default: break;
        }
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          _type_pool->make_type<sem_type::PrimitiveType>(target_prime),
          val
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to unsigned integer");
      }
    } break;


    case TypePrime::kF32: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          _type_pool->make_type<sem_type::PrimitiveType>(target_prime),
          static_cast<float>(arg)
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to floating point");
      }
    } break;
    case TypePrime::kF64: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
          _type_pool->make_type<sem_type::PrimitiveType>(target_prime),
          static_cast<double>(arg)
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to floating point");
      }
    } break;

    default:
      _recorder->tagged_report(kErrTag, "Unsupported target type in cast expression");
    }

  }, primitive->value);
}

void ConstEvaluator::postVisit(GroupedExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  node.set_const_value(*node.expr()->const_value());
}

}
