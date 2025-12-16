#include "syntax_check.h"

#include <limits>
#include <cmath>

namespace insomnia::rust_shard::ast {
/*************************** ConstEvaluator ***************************************/

void ConstEvaluator::postVisit(LiteralExpression &node) {
  using stype::PrimitiveType;
  auto prime = node.prime();
  stype::TypePtr type = _type_pool->make_type<PrimitiveType>(prime);
  std::visit([&](auto &&arg) {
    sconst::ConstPrimitive primitive_val(prime, arg);
    node.set_const_value(
      _const_pool->make_const<sconst::ConstPrimitive>(type, primitive_val)
    );
  }, node.spec_value());
}

const std::string ConstEvaluator::kErrTag = "ConstEvaluator Failure";

void ConstEvaluator::postVisit(BorrowExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = node.expr()->const_value();
  auto tp = inner->type();
  if(node.is_mut()) tp = _type_pool->make_type<stype::MutType>(tp);
  tp = _type_pool->make_type<stype::RefType>(inner->type());
  node.set_const_value(_const_pool->make_const<sconst::ConstReference>(
    tp,
    inner
  ));
}

void ConstEvaluator::postVisit(DereferenceExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = node.expr()->const_value();
  if(auto ref = inner->get_if<sconst::ConstReference>()) {
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
  auto inner = node.expr()->const_value();
  auto primitive = inner->get_if<sconst::ConstPrimitive>();
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
      auto prime = inner->type().get<stype::PrimitiveType>()->prime();
      if constexpr(type_utils::is_one_of<T, std::int64_t>) {
        using stype::TypePrime;
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
      node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
        inner->type(), // still primitive
        prime,
        -arg // negation
      ));
    } else if constexpr(std::is_same_v<T, bool>) {
      if(node.oper() != Operator::kLogicalNot) {
        _recorder->tagged_report(kErrTag, "Invalid operator kLogicalNot in negation expression");
      }
      node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
        inner->type(), // still primitive
        stype::TypePrime::kBool, // ???
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
  auto inner1 = node.expr1()->const_value();
  auto inner2 = node.expr2()->const_value();
  auto prime1 = inner1->get_if<sconst::ConstPrimitive>();
  auto prime2 = inner2->get_if<sconst::ConstPrimitive>();
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
            node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
              inner1->type(), arg1 << arg2));
          } else {
            node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
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

  std::visit([&]<typename T01, typename T02>(T01 &&arg1, T02 &&arg2) {
    using T1 = std::decay_t<T01>;
    using T2 = std::decay_t<T02>;
    if constexpr(!std::is_same_v<T1, T2>) {
      throw std::runtime_error("Type mismatch not checked, inner design has flaws.");
    } else {
      using T = T1;
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        if((oper == Operator::kDiv || oper == Operator::kMod) && arg2 == 0) {
          _recorder->tagged_report(kErrTag, "Division by zero");
          return;
        }
        auto prime1 = inner1->type().get<stype::PrimitiveType>()->prime();
        switch(oper) {
        case Operator::kAdd:
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 + arg2));
          break;
        case Operator::kSub:
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 - arg2));
          break;
        case Operator::kMul:
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 * arg2));
          break;
        case Operator::kDiv:
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 / arg2));
          break;
        case Operator::kMod: {
          T val = 0;
          if constexpr(std::is_floating_point_v<T>) {
            val = std::fmod(arg1, arg2);
          } else {
            val = arg1 % arg2;
          }
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, val));
        } break;
        case Operator::kBitwiseAnd:
        case Operator::kBitwiseOr:
        case Operator::kBitwiseXor:
          if constexpr(std::is_integral_v<T>) {
            if(oper == Operator::kBitwiseAnd)
              node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 & arg2));
            else if(oper == Operator::kBitwiseOr)
              node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 | arg2));
            else
              node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), prime1, arg1 ^ arg2));
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
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), stype::TypePrime::kBool, arg1 && arg2));
        } else if(oper == Operator::kLogicalOr) {
          node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(inner1->type(), stype::TypePrime::kBool, arg1 || arg2));
        } else {
          _recorder->tagged_report(kErrTag, "Invalid operator for arithmetic/logical expression (boolean logical)");
        }
      } else {
        throw std::runtime_error("Unsupported primitive type in arithmetic/logical expression");
      }
    }
  }, prime1->value, prime2->value);
}

void ConstEvaluator::postVisit(ComparisonExpression &node) {
  if(!node.expr1()->has_constant() || !node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner1 = node.expr1()->const_value();
  auto inner2 = node.expr2()->const_value();
  auto prime1 = inner1->get_if<sconst::ConstPrimitive>();
  auto prime2 = inner2->get_if<sconst::ConstPrimitive>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in comparison expression");
    return;
  }
  if(inner1->type() != inner2->type()) {
    _recorder->tagged_report(kErrTag, "Mismatched types in comparison expression");
    return;
  }

  std::visit([&]<typename T01, typename T02>(T01 &&arg1, T02 &&arg2) {
    using T1 = std::decay_t<T01>;
    using T2 = std::decay_t<T02>;
    if constexpr(!std::is_same_v<T1, T2>) {
      throw std::runtime_error("Type mismatch not checked, inner design has flaws.");
    } else {
      using T = T01;
      bool result = false;
      auto oper = node.oper();
      if constexpr(type_utils::is_one_of<T, char, std::int64_t, std::uint64_t, float, double, bool>) {
        switch(oper) {
        case Operator::kEq: result = (arg1 == arg2); break;
        case Operator::kNe: result = (arg1 != arg2); break;
        case Operator::kGt: result = (arg1 > arg2); break;
        case Operator::kLt: result = (arg1 < arg2); break;
        case Operator::kGe: result = (arg1 >= arg2); break;
          case Operator::kLe: result = (arg1 <= arg2);
            break;
          default:
          _recorder->tagged_report(kErrTag, "Invalid operator in comparison expression");
          return;
        }
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(stype::TypePrime::kBool),
          stype::TypePrime::kBool,
          result
        ));
      } else {
        throw std::runtime_error("Unsupported primitive type in comparison expression");
      }
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
  auto inner1 = node.expr1()->const_value();
  auto prime1 = inner1->get_if<sconst::ConstPrimitive>();
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
    node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
      _type_pool->make_type<stype::PrimitiveType>(stype::TypePrime::kBool),
      stype::TypePrime::kBool,
      false));
    return;
  }
  if(oper == Operator::kLogicalOr && *result1) {
    node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
      _type_pool->make_type<stype::PrimitiveType>(stype::TypePrime::kBool),
      stype::TypePrime::kBool,
      true));
    return;
  }

  node.expr2()->accept(*this);
  if(!node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner2 = node.expr2()->const_value();
  auto prime2 = inner2->get_if<sconst::ConstPrimitive>();
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
  auto inner = node.expr()->const_value();
  auto primitive = inner->get_if<sconst::ConstPrimitive>();
  if(!primitive) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in type cast expression");
    return;
  }

  auto target_type = node.type_no_bounds()->get_type().get_if<stype::PrimitiveType>();
  if(!target_type) {
    _recorder->tagged_report(kErrTag, "Target type not primitive in type cast expression");
    return;
  }
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    using stype::TypePrime;
    auto target_prime = target_type->prime();
    switch(target_prime) {
    case TypePrime::kBool:
      if constexpr(type_utils::is_one_of<T, bool>) {
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(TypePrime::kBool),
          TypePrime::kBool,
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
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(target_prime),
          target_prime,
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
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(target_prime),
          target_prime,
          val
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to unsigned integer");
      }
    } break;


    case TypePrime::kF32: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(target_prime),
          target_prime,
          static_cast<float>(arg)
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to floating point");
      }
    } break;
    case TypePrime::kF64: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_const_value(_const_pool->make_const<sconst::ConstPrimitive>(
          _type_pool->make_type<stype::PrimitiveType>(target_prime),
          target_prime,
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
  node.set_const_value(node.expr()->const_value());
}
}

namespace insomnia::rust_shard::ast {

/********************** TypeFiller *****************************/

const std::string TypeFiller::kErrTypeNotResolved = "Error: Type not resolved";
const std::string TypeFiller::kErrTypeNotMatch = "Error: Type not match between evaluation and declaration";
const std::string TypeFiller::kErrConstevalFailed = "Error: Necessary const evaluation failed";
const std::string TypeFiller::kErrIdentNotResolved = "Error: Identifier not resolved as expected";

void TypeFiller::postVisit(Function &node) {
  // check whether every control block share same return types.
  // allow never type and self type.
  if(node.body_opt() && !node.body_opt()->get_type()) {
    _recorder->tagged_report(kErrTypeNotResolved, "Function body type not resolved");
    return;
  }
  if(node.res_type_opt() && !node.res_type_opt()->get_type()) {
    _recorder->tagged_report(kErrTypeNotResolved, "Function return type hint not resolved");
    return;
  }

  stype::TypePtr ret_type;

  if(node.body_opt() && node.res_type_opt()) {
    if(node.body_opt()->get_type() != node.res_type_opt()->get_type()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Function body type and return type hint not match");
      return;
    }
    ret_type = node.body_opt()->get_type();
  } else if(node.body_opt()) {
    ret_type = node.body_opt()->get_type();
  } else if(node.res_type_opt()) {
    ret_type = node.res_type_opt()->get_type();
  } else {
    _recorder->tagged_report(kErrTypeNotResolved, "Function failed to deduct its return type");
  }

  // after checking, register this function type to the symbol table.
  auto func_symbol = find_symbol(node.ident());
  if(!func_symbol) {
    throw std::runtime_error("Fatal error: invalid function name not checked");
  }

  std::vector<stype::TypePtr> params;
  if(auto ps = node.params_opt().get()) {
    if(auto s = ps->self_param_opt().get()) {
      // self param
      auto pt = _type_pool->make_type<stype::SelfType>();
      if(s->is_mut()) pt = _type_pool->make_type<stype::MutType>(pt);
      if(s->is_ref()) pt = _type_pool->make_type<stype::RefType>(pt);
      params.push_back(pt);
    }
    // other params
    for(const auto &p: ps->func_params()) {
      stype::TypePtr pt;
      if(auto fpt = dynamic_cast<FunctionParamType*>(p.get())) {
        pt = fpt->type()->get_type();
      } else if(auto fpp = dynamic_cast<FunctionParamPattern*>(p.get())) {
        pt = fpp->type()->get_type();
      }
      if(!pt) {
        throw std::runtime_error("Function param invalidness not detected");
      }
      params.push_back(pt);
    }
  }

  auto func = _type_pool->make_type<stype::FunctionType>(node.ident(), std::move(params), ret_type);
  func_symbol->type = func;
}

void TypeFiller::postVisit(StructStruct &node) {
  auto info = find_symbol(node.ident());
  std::map<StringRef, stype::TypePtr> struct_fields;
  if(node.fields_opt()) {
    for(const auto &field: node.fields_opt()->fields()) {
      auto ident = field->ident();
      auto ast_type = field->type()->get_type();
      if(!ast_type) {
        _recorder->report("Unresolved struct field type");
        continue; // continue partial compiling
      }
      struct_fields.emplace(ident, ast_type);
    }
  }
  info->type.get<stype::StructType>()->set_fields(std::move(struct_fields));
}

void TypeFiller::postVisit(Enumeration &node) {
  auto info = find_symbol(node.ident());
  auto raw_enum_type = info->type.get_if<stype::EnumType>();
  if(node.items_opt()) {
    for(const auto &item: node.items_opt()->items()) {
      if(item->discr_opt()) {
        _recorder->report("EnumItemDiscrimination not implemented. Ignoring it");
      }
    }
  }
}

void TypeFiller::postVisit(EnumItem &node) {
  throw std::runtime_error("EnumItem not implemented");
  return;
}

void TypeFiller::postVisit(Trait &node) {
  // function, constant and type alias

  auto trait = _type_pool->make_type<stype::TraitType>(node.ident());
  for(const auto &asso: node.asso_items()) {
    if(auto fp = dynamic_cast<AssociatedFunction*>(asso.get())) {
      auto f = fp->func().get();
      // must have explicit return type
      // if return type not "Self", then must be determined
      // if returns "Self", then record it.
    } else if(auto ap = dynamic_cast<AssociatedTypeAlias*>(asso.get())) {

    } else if(auto cp = dynamic_cast<AssociatedConstantItem*>(asso.get())) {

    } else {
      throw std::runtime_error("Invalid Asso item not detected");
    }
  }
  // function must have explicit return types
}

void TypeFiller::postVisit(TypeAlias &node) {
  auto info = find_symbol(node.ident());
  auto alias_type = info->type.get_if<stype::AliasType>();
  if(node.type_opt()) {
    auto t = node.type_opt()->get_type();
    if(!t) {
      _recorder->report("Type not set");
      return;
    }
    node.set_type(t);
    alias_type->set_type(t); // bind alias and its underlying type
  }
}

void TypeFiller::postVisit(ParenthesizedType &node) {
  auto type = node.type()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside parenthesis");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(TupleType &node) {
  std::vector<stype::TypePtr> types;
  for(const auto &item: node.types()) {
    auto type = item->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside tuple");
      return;
    }
    types.push_back(type);
  }
  node.set_type(_type_pool->make_type<stype::TupleType>(std::move(types)));
}

void TypeFiller::postVisit(ReferenceType &node) {
  auto type = node.type_no_bounds()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside reference chain");
    return;
  }
  if(node.is_mut()) type = _type_pool->make_type<stype::MutType>(type);
  type = _type_pool->make_type<stype::RefType>(type);
  node.set_type(type);
}

void TypeFiller::postVisit(ArrayType &node) {
  auto type = node.type()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside array");
    return;
  }
  constEvaluate(*node.const_expr());
  if(!node.const_expr()->has_constant()) {
    _recorder->tagged_report(kErrTypeNotResolved, "non-evaluable array length");
    return;
  }
  auto cval = node.const_expr()->const_value()->get_if<sconst::ConstPrimitive>();
  if(!cval) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not a primitive type");
    return;
  }
  auto length = cval->get_usize();
  if(!length) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not an index value");
    return;
  }
  node.set_type(_type_pool->make_type<stype::ArrayType>(std::move(type), *length));
}

void TypeFiller::postVisit(SliceType &node) {
  _recorder->tagged_report(kErrTypeNotResolved, "Slice not supported yet");
}

void TypeFiller::postVisit(TypePath &node) {
  // Enumeration/crate/generics
  // Since we only support single crate / no generic / simple enumeration,
  // we only have to check enumeration items.
  if(node.is_absolute()) {
    // ... I don't know.
  }
  for(auto it = node.segments().begin(); it != node.segments().end(); ++it) {
    // PathIdentSegment ->
    // IDENTIFIER | "super" | "self" | "Self" | "crate"
    auto ident = (*it)->ident_segment()->ident();
    if(ident == "super") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"super\" is not supported yet");
      return;
    } else if(ident == "self") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"self\" is not supported yet");
      return;
    } else if(ident == "Self") {
      // set a temporary SelfType here.
      node.set_type(_type_pool->make_type<stype::SelfType>());
      return;
    } else if(ident == "crate") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"crate\" is not supported yet");
      return;
    }
    auto info = find_symbol(ident);
    if(!info) {
      _recorder->tagged_report(kErrTypeNotResolved, "TypePath identifier unrecognizable");
      return;
    }
    switch(info->kind) {
    case SymbolKind::kConstant: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Constant type not determined");
        return;
      }
      auto ait = it; ++ait;
      if(ait != node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Constant as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kEnum: {
      // check whether the next one is a valid field.
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Enum type not determined");
        return;
      }
      ++it;
      if(it == node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Enum variant error, expected an enum item");
        return;
      }
      auto ait = it; ++ait;
      if(ait != node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Enum variant as a type path interval");
        return;
      }
      auto item_ident = (*it)->ident_segment()->ident();
      auto enum_type = info->type.get_if<stype::EnumType>();
      if(!enum_type) {
        throw std::runtime_error("Fatal error: enum type mismatch undetected");
      }
      if(auto vit = enum_type->variants().find(item_ident); vit == enum_type->variants().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Not a valid enum variant");
        return;
      } else if(!vit->second) {
        throw std::runtime_error("Fatal error: enum variant type not set");
      } else {
        node.set_type(stype::TypePtr(vit->second));
      }
    } break;
    case SymbolKind::kStruct: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Struct type not determined");
        return;
      }
      auto ait = it; ++ait;
      if(ait != node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Struct as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kPrimitiveType: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "PrimitiveType type not determined");
        return;
      }
      auto ait = it; ++ait;
      if(ait != node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "PrimitiveType as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kTypeAlias: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "TypeAlias type not determined");
        return;
      }
      auto ait = it; ++ait;
      if(ait != node.segments().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "TypeAlias as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kFunction:
    case SymbolKind::kTrait:
    case SymbolKind::kVariable: {
      // ???
      throw std::runtime_error("TypePath for function/trait/variable not implemented yet");
      return;
    } break;
    }
  }
  if(!node.get_type()) {
    throw std::runtime_error("Fatal error: type not set in TypePath");
  }
}

void TypeFiller::postVisit(LiteralExpression &node) {
  node.set_type(_type_pool->make_type<stype::PrimitiveType>(node.prime()));
}

void TypeFiller::postVisit(PathInExpression &node) {
  if(node.segments().size() != 1) {
    throw std::runtime_error("PathInExpression with path sep(s) not implemented yet");
    return;
  }
  auto ident = node.segments().back()->ident_seg()->ident();
  if(ident == "super" || ident == "self" || ident == "Self" || ident == "crate") {
    _recorder->tagged_report(kErrIdentNotResolved, "Not expected keywords here in end of PathInExpression");
    return;
  }
  auto info = find_symbol(ident);
  if(!info) {
    _recorder->tagged_report(kErrIdentNotResolved, "Identifier not found as the end of PathInExpression");
    return;
  }
  if(info->kind == SymbolKind::kConstant || info->kind == SymbolKind::kVariable || info->kind == SymbolKind::kFunction) {
    node.set_type(info->type);
  } else {
    _recorder->tagged_report(kErrIdentNotResolved, "Invalid type associated with the identifier");
    return;
  }
}

void TypeFiller::postVisit(BorrowExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  if(node.is_mut()) type = _type_pool->make_type<stype::MutType>(type);
  type = _type_pool->make_type<stype::RefType>(type);
  node.set_type(type);
}

void TypeFiller::postVisit(DereferenceExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  if(auto inner = type.get_if<stype::MutType>()) type = stype::TypePtr(inner);
  if(auto inner = type.get_if<stype::RefType>()) {
    type = stype::TypePtr(inner);
  } else {
    _recorder->tagged_report(kErrTypeNotMatch, "Type not a reference");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(NegationExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(std::move(type));
}

void TypeFiller::postVisit(ArithmeticOrLogicalExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kShl:
  case Operator::kShr:
    if(prime1->is_integer() && prime2->is_unsigned()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator << >>");
      return;
    }
    break;

  case Operator::kAdd:
  case Operator::kSub:
  case Operator::kMul:
  case Operator::kDiv:
  case Operator::kMod:
    if(prime1->prime() == prime2->prime()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator + - * / %");
      return;
    }
    break;

  case Operator::kBitwiseAnd:
  case Operator::kBitwiseOr:
  case Operator::kBitwiseXor:
    if(prime1->prime() == prime2->prime() && prime1->is_integer()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator & | ^");
      return;
    }
    break;

  case Operator::kLogicalAnd:
  case Operator::kLogicalOr:
    if(prime1->prime() == prime2->prime() && prime1->prime() == stype::TypePrime::kBool) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator && ||");
      return;
    }
    break;

  default:
    throw std::runtime_error("Invalid operator type in arithmetic/logical expression");
  }
}

void TypeFiller::postVisit(ComparisonExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kEq:
  case Operator::kNe:
  case Operator::kGt:
  case Operator::kLt:
  case Operator::kGe:
  case Operator::kLe:
    if(prime1->prime() == prime2->prime()) {
      node.set_type(_type_pool->make_type<stype::PrimitiveType>(stype::TypePrime::kBool));
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator < > <= >= == !=");
      return;
    }
    break;
  default:
    throw std::runtime_error("Invalid operator type in comparison expression");
  }
}

void TypeFiller::postVisit(LazyBooleanExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kLogicalAnd:
  case Operator::kLogicalNot:
    if(prime1->prime() == prime2->prime() && prime1->prime() == stype::TypePrime::kBool) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator && ||");
      return;
    }
    break;
  default:
    throw std::runtime_error("Invalid operator type in lazy boolean expression");
  }
}

void TypeFiller::postVisit(TypeCastExpression &node) {
  auto from_type = node.expr()->get_type();
  if(!from_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto to_type = node.type_no_bounds()->get_type();
  if(!to_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto from_prime = from_type.get_if<stype::PrimitiveType>();
  auto to_prime = to_type.get_if<stype::PrimitiveType>();
  if(!from_prime || !to_prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(to_prime->prime()) {
    using stype::TypePrime;

  case TypePrime::kI8:
  case TypePrime::kI16:
  case TypePrime::kI32:
  case TypePrime::kI64:
  case TypePrime::kISize:
  case TypePrime::kU8:
  case TypePrime::kU16:
  case TypePrime::kU32:
  case TypePrime::kU64:
  case TypePrime::kUSize:
  case TypePrime::kF32:
  case TypePrime::kF64:
    if(from_prime->is_integer() || from_prime->is_floating_point() ||
      from_prime->prime() == TypePrime::kChar || from_prime->prime() == TypePrime::kBool) {
      node.set_type(from_type);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Cannot cast type to integer/floating point");
      return;
    }
    break;
  default:
    if(from_prime->prime() == to_prime->prime()) {
      node.set_type(to_type);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Cannot perform type cast to other type");
      return;
    }
  }
}

void TypeFiller::postVisit(AssignmentExpression &node) {
  auto to_type = node.expr1()->get_type();
  if(to_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto from_type = node.expr2()->get_type();
  if(!from_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  if(from_type == to_type) {
    node.set_type(to_type);
  } else {
    _recorder->tagged_report(kErrTypeNotMatch, "Cannot perform assignment to other type");
    return;
  }
}

void TypeFiller::postVisit(CompoundAssignmentExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kShlAssign:
  case Operator::kShrAssign:
    if(prime1->is_integer() && prime2->is_unsigned()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator <<= >>=");
      return;
    }
    break;

  case Operator::kAddAssign:
  case Operator::kSubAssign:
  case Operator::kMulAssign:
  case Operator::kDivAssign:
  case Operator::kModAssign:
    if(prime1->prime() == prime2->prime()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator += -= *= /= %=");
      return;
    }
    break;

  case Operator::kBitwiseAndAssign:
  case Operator::kBitwiseOrAssign:
  case Operator::kBitwiseXorAssign:
    if(prime1->prime() == prime2->prime() && prime1->is_integer()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator &= |= ^=");
      return;
    }
    break;

  default:
    throw std::runtime_error("Invalid operator type in compound assignment expression");
  }
}

void TypeFiller::postVisit(GroupedExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(ArrayExpression &node) {
  if(!node.elements_opt()) {
    _recorder->tagged_report(kErrTypeNotResolved, "Empty array not supported yet");
    return;
  }
  if(node.elements_opt()->is_explicit()) {
    auto arr = dynamic_cast<ExplicitArrayElements*>(node.elements_opt().get());
    if(!arr) {
      throw std::runtime_error("Explicit array collapsed");
      return;
    }
    if(arr->expr_list().empty()) {
      throw std::runtime_error("Explicit array emptiness not checked");
      return;
    }
    auto type = arr->expr_list().front()->get_type();
    for(auto &elem: arr->expr_list()) {
      auto t = elem->get_type();
      if(!t) {
        _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
        return;
      }
      if(type != t) {
        _recorder->tagged_report(kErrTypeNotMatch, "Array type not same");
        return;
      }
    }
    node.set_type(_type_pool->make_type<stype::ArrayType>(type, arr->expr_list().size()));
  } else {
    auto arr = dynamic_cast<RepeatedArrayElements*>(node.elements_opt().get());
    if(!arr) {
      throw std::runtime_error("Repeated array collapsed");
      return;
    }
    constEvaluate(*arr->len_expr());
    if(!arr->len_expr()->has_constant()) {
      _recorder->tagged_report(kErrConstevalFailed, "Type length not evaluated");
      return;
    }
    auto length_prime = arr->len_expr()->const_value()->get_if<sconst::ConstPrimitive>();
    if(!length_prime) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not a primitive");
      return;
    }
    auto length = std::get_if<stype::usize_t>(&length_prime->value);
    if(!length) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not an unsigned value");
      return;
    }
    auto type = arr->val_expr()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    node.set_type(_type_pool->make_type<stype::ArrayType>(type, *length));
  }
}

void TypeFiller::postVisit(IndexExpression &node) {
  auto expr_type = node.expr_obj()->get_type();
  if(!expr_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto index_type = node.expr_index()->get_type();
  if(!index_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto expr_arr = expr_type.get_if<stype::ArrayType>();
  if(!expr_arr) {
    _recorder->tagged_report(kErrTypeNotMatch, "Index on not an array");
    return;
  }
  auto index_primitive = index_type.get_if<stype::PrimitiveType>();
  if(!index_primitive || index_primitive->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Index not an integer");
    return;
  }
  // out_of_range is a runtime problem
  node.set_type(expr_arr->type());
}

void TypeFiller::postVisit(TupleExpression &node) {
  std::vector<stype::TypePtr> members;
  if(node.elems_opt()) for(const auto &expr: node.elems_opt()->expr_list()) {
    auto t = expr->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    members.push_back(std::move(t));
  }
  node.set_type(_type_pool->make_type<stype::TupleType>(std::move(members)));
}

void TypeFiller::postVisit(TupleIndexingExpression &node) {
  throw std::runtime_error("TupleIndexExpression not supported");
  return;
}

void TypeFiller::postVisit(StructExpression &node) {
  throw std::runtime_error("StructExpression not supported");
  return;
}

void TypeFiller::postVisit(CallExpression &node) {
  auto func_type = node.expr()->get_type().get_if<stype::FunctionType>();
  if(!func_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Function call type not resolved");
    return;
  }
  bool flag = true;
  if(!node.params_opt()) {
    // no params.
    if(!func_type->params().empty()) {
      flag = false;
    }
  } else {
    const auto &exprs = node.params_opt()->expr_list();
    const auto &params = func_type->params();
    if(exprs.size() != params.size()) {
      flag = false;
    } else {
      for(int i = 0; i < exprs.size(); ++i) {
        if(params[i] != exprs[i]->get_type()) {
          flag = false; break;
        }
      }
    }
  }
  if(!flag) {
    _recorder->tagged_report(kErrTypeNotMatch, "Function expect no param");
    return;
  }
  node.set_type(func_type->return_type());
}

void TypeFiller::postVisit(MethodCallExpression &node) {
  throw std::runtime_error("MethodCallExpression not supported");
  return;
}

void TypeFiller::postVisit(FieldExpression &node) {
  throw std::runtime_error("FieldExpression not supported");
  return;
}

void TypeFiller::postVisit(ContinueExpression &node) {
  // ???
  // always success
}

void TypeFiller::postVisit(BreakExpression &node) {
  if(!node.expr_opt()) {
    node.set_type(_type_pool->make_unit());
    return;
  }
  auto type = node.expr_opt()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(RangeExpr &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime1->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeExpr");
    return;
  }
  node.set_type(_type_pool->make_type<stype::RangeType>(type1));
}

void TypeFiller::postVisit(RangeFromExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<stype::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeFromExpr");
    return;
  }
  node.set_type(_type_pool->make_type<stype::RangeType>(type));
}

void TypeFiller::postVisit(RangeToExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<stype::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeToExpr");
    return;
  }
  node.set_type(_type_pool->make_type<stype::RangeType>(type));
}

void TypeFiller::postVisit(RangeFullExpr &node) {
  throw std::runtime_error("RangeFullExpr not supported");
  return;
}

void TypeFiller::postVisit(RangeInclusiveExpr &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimitiveType>();
  auto prime2 = type2.get_if<stype::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime1->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeInclusiveExpr");
    return;
  }
  node.set_type(_type_pool->make_type<stype::RangeType>(type1));
}

void TypeFiller::postVisit(RangeToInclusiveExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<stype::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeToInclusiveExpr");
    return;
  }
  node.set_type(_type_pool->make_type<stype::RangeType>(type));
}

void TypeFiller::postVisit(ReturnExpression &node) {
  if(!node.expr_opt()) {
    node.set_type(_type_pool->make_unit());
    return;
  }
  auto type = node.expr_opt()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(UnderscoreExpression &node) {
  throw std::runtime_error("UnderscoreExpression not supported");
  return;
}

void TypeFiller::postVisit(BlockExpression &node) {
  if(!node.stmts_opt() || !node.stmts_opt()->expr_opt()) {
    node.set_type(_type_pool->make_unit());
    return;
  }
  auto type = node.stmts_opt()->expr_opt()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(FunctionBodyExpr &node) {
  auto type = _type_pool->make_unit();
  if(node.stmts_opt() && node.stmts_opt()->expr_opt()) {
    type = node.stmts_opt()->expr_opt()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
  }
  for(const auto &elem: node.func_returns()) {
    auto t = elem->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    if(t != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "FunctionBodyExpression has different return type");
      return;
    }
  }
  node.set_type(type);
}

void TypeFiller::postVisit(InfiniteLoopExpression &node) {
  if(node.loop_breaks().empty()) {
    node.set_type(_type_pool->make_type<stype::NeverType>());
    return;
  }
  auto type = node.loop_breaks().front()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  for(const auto &elem: node.loop_breaks()) {
    // compared first element again... never mind
    auto t = elem->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    if(t != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "InfiniteLoopExpression has different return type");
      return;
    }
  }
  node.set_type(type);
}

void TypeFiller::postVisit(PredicateLoopExpression &node) {
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimitiveType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }

  auto type = _type_pool->make_unit();
  if(node.block_expr()->stmts_opt() && node.block_expr()->stmts_opt()->expr_opt()) {
    type = node.block_expr()->stmts_opt()->expr_opt()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
  }
  for(const auto &elem: node.loop_breaks()) {
    // compared first element again... never mind
    auto t = elem->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    if(t != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "PredicateLoopExpression has different return type");
      return;
    }
  }
  node.set_type(type);
}

void TypeFiller::postVisit(IfExpression &node) {
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimitiveType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }

  auto type = node.block_expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  bool success = true;
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    if constexpr(!std::is_same_v<T, std::monostate>) {
      // T = std::unique_ptr<BlockExpression/IfExpression>
      auto t = arg->get_type();
      if(!t || t != type) {
        success = false;
      }
    }
  }, node.else_spec());
  if(!success) {
    _recorder->tagged_report(kErrTypeNotMatch, "Type not match between if blocks");
    return;
  }
  node.set_type(type);
}

void TypeFiller::postVisit(MatchExpression &node) {
  // all match branches must have the same result type
  // that is the type of this expression

  // extra check in this stage:
  // ... too much.
  throw std::runtime_error("MatchExpression not implemented yet");
}

void TypeFiller::postVisit(LetStatement &node) {
  if(!node.expr_opt()) {
    _recorder->report("Uninitialized variable");
    return;
  }
  auto type = node.expr_opt()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside let statement");
    return;
  }
  if(node.type_opt()) {
    auto type_tag = node.type_opt()->get_type();
    if(type_tag && type_tag != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "type mismatch inside let statement."
        " Type tag: " + type_tag->to_string() + "; Expr type: " + type->to_string());
      return;
    }
  }
  bind_pattern(node.pattern().get(), type);
}

void TypeFiller::bind_pattern(PatternNoTopAlt *pattern, stype::TypePtr type) {
  if(auto ident_p = dynamic_cast<IdentifierPattern*>(pattern)) {
    bind_identifier(ident_p, type);
  } else if(auto wildcard_p = dynamic_cast<WildcardPattern*>(pattern)) {
    bind_wildcard(wildcard_p, type);
  } else if(auto tuple_p = dynamic_cast<TuplePattern*>(pattern)) {
    bind_tuple(tuple_p, type);
  } else if(auto struct_p = dynamic_cast<StructPattern*>(pattern)) {
    bind_struct(struct_p, type);
  } else if(auto ref_p = dynamic_cast<ReferencePattern*>(pattern)) {
    bind_reference(ref_p, type);
  } else if(auto literal_p = dynamic_cast<LiteralPattern*>(pattern)) {
    bind_literal(literal_p, type);
  } else if(auto grouped_p = dynamic_cast<GroupedPattern*>(pattern)) {
    bind_grouped(grouped_p, type);
  } else if(auto slice_p = dynamic_cast<SlicePattern*>(pattern)) {
    bind_slice(slice_p, type);
  } else if(auto path_p = dynamic_cast<PathPattern*>(pattern)) {
    bind_path(path_p, type);
  } else {
    _recorder->tagged_report(kErrTypeNotResolved, "Unsupported type in resolution");
  }
}

void TypeFiller::bind_identifier(IdentifierPattern *pattern, stype::TypePtr type) {
  // always success
  bool is_mut = pattern->is_mut();
  if(pattern->is_ref()) {
    // let ref y = r <=> let y = &r
    // let ref mut z = r <=> let z = &mut r
    if(is_mut) type = _type_pool->make_type<stype::MutType>(type);
    type = _type_pool->make_type<stype::RefType>(type);
    is_mut = false; // z is immutable (while *z is mutable)
  }
  auto info = add_symbol(pattern->ident(), SymbolInfo{
    .node = pattern, .ident = pattern->ident(), .kind = SymbolKind::kVariable,
    .type = type
  });
  if(!info) {
    _recorder->report("Variable identifier already defined");
    return;
  }
}

void TypeFiller::bind_wildcard(WildcardPattern *pattern, stype::TypePtr type) {
  // always success
  // nothing to do here
}

void TypeFiller::bind_tuple(TuplePattern *pattern, stype::TypePtr type) {
  // let (a, b) = (1, 2) -> a = 1, b = 2
  auto t = type.get_if<stype::TupleType>();
  if(!t) {
    _recorder->report("Type binding failed: not a tuple");
    return;
  }
  std::size_t pattern_size = 0;
  if(pattern->items_opt()) pattern_size = pattern->items_opt()->patterns().size();
  if(t->members().size() != pattern_size) {
    _recorder->report("Type binding failed: tuple length mismatch");
    return;
  }
  for(std::size_t i = 0; i < pattern_size; ++i) {
    auto &sub_pattern = pattern->items_opt()->patterns()[i];
    auto sub_type = t->members()[i];
    if(sub_pattern->patterns().size() != 1) {
      _recorder->report("Type binding failed: multi patterns not supported yet");
      return;
    }
    bind_pattern(sub_pattern->patterns()[i].get(), sub_type);
  }
}

void TypeFiller::bind_struct(StructPattern *pattern, stype::TypePtr type) {
  // let Type { x1: x, y1: y } = Type{ x1: 1, y1: 2, z1: 3 } -> x = 1, y = 2
  auto t = type.get_if<stype::StructType>();
  if(!t) {
    _recorder->report("Type binding failed: not a struct");
    return;
  }
  if(pattern->path_in_expr()->segments().back()->ident_seg()->ident() != t->ident()) {
    _recorder->report("Type binding failed: not the same struct");
    return;
  }
  if(pattern->elems_opt()) for(auto &field: pattern->elems_opt()->fields()->fields()) {
    auto sub_pattern = field->pattern().get();
    if(sub_pattern->patterns().size() != 1) {
      _recorder->report("Type binding failed: multi patterns not supported yet");
      return;
    }
    auto id = field->ident();
    if(auto it = t->fields().find(id); it != t->fields().end()) {
      bind_pattern(sub_pattern->patterns().front().get(), it->second);
    } else {
      _recorder->report("Type binding failed: field identifier not exist");
    }
  }
}

void TypeFiller::bind_reference(ReferencePattern *pattern, stype::TypePtr type) {
  if(auto t = type.get_if<stype::RefType>(); !t) {
    _recorder->report("Type binding failed: not a reference");
    return;
  } else {
    type = t->type();
  }
  if(pattern->is_mut()) {
    if(auto t = type.get_if<stype::MutType>(); !t) {
      _recorder->report("Type binding failed: reference mutability mismatch");
      return;
    } else {
      type = t->type();
    }
  }
  auto sub_pattern = pattern->pattern().get();
  bind_pattern(sub_pattern, type);
}

void TypeFiller::bind_literal(LiteralPattern *pattern, stype::TypePtr type) {
  _recorder->report("Type binding error: literal pattern not supported in let statement");
}

void TypeFiller::bind_grouped(GroupedPattern *pattern, stype::TypePtr type) {
  auto sub_pattern = pattern->pattern().get();
  if(sub_pattern->patterns().size() != 1) {
    _recorder->report("Type binding failed: multi pattern not supported yet");
    return;
  }
  bind_pattern(sub_pattern->patterns().front().get(), type);
}

void TypeFiller::bind_slice(SlicePattern *pattern, stype::TypePtr type) {
  _recorder->report("Slice Not supported yet");
}

void TypeFiller::bind_path(PathPattern *pattern, stype::TypePtr type) {
  _recorder->report("Type binding error: path pattern not supported in let statement");
}

}





























