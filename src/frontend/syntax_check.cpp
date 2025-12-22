#include "syntax_check.h"

#include <limits>
#include <cmath>

namespace insomnia::rust_shard::ast {
/*************************** ConstEvaluator ***************************************/

void ConstEvaluator::preVisit(AssignmentExpression &node) {
  node.expr1()->set_lside();
}

void ConstEvaluator::preVisit(CompoundAssignmentExpression &node) {
  node.expr1()->set_lside();
}

void ConstEvaluator::postVisit(LiteralExpression &node) {
  using stype::PrimeType;
  auto prime = node.prime();
  stype::TypePtr type = _type_pool->make_type<PrimeType>(prime);
  std::visit([&](auto &&arg) {
    sconst::ConstPrime primitive_val(prime, arg);
    node.set_cval(
      _const_pool->make_const<sconst::ConstPrime>(type, primitive_val)
    );
  }, node.spec_value());
}

const std::string ConstEvaluator::kErrTag = "ConstEvaluator Failure";

void ConstEvaluator::postVisit(BorrowExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  // &x: x must be a lvalue
  // &mut x: x must be a PlaceMut lvalue
  if(!node.expr()->is_lside()) {
    _recorder->tagged_report(kErrTag, "Target not referable");
    return;
  }
  auto cval = node.expr()->cval();
  auto tp = cval->type();
  bool is_place_mut = false;
  if(node.is_mut() && !is_place_mut) {
    _recorder->tagged_report(kErrTag, "Target not mutably referable");
    return;
  }
  tp = _type_pool->make_type<stype::RefType>(tp, node.is_mut());
  cval = _const_pool->make_const<sconst::ConstRef>(tp, cval, node.is_mut());
  node.set_cval(cval);
}

void ConstEvaluator::postVisit(DereferenceExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  // *((mut) &x) -> x (rside), invalid (lside)
  // *((mut) &mut x) -> x (rside), x (lside)
  auto cval = node.expr()->cval();
  auto tp = cval->type();
  if(auto r = tp.get_if<stype::RefType>()) {
    if(node.is_lside() && !r->ref_is_mut()) {
      _recorder->tagged_report(kErrTag, "Dereferencing " + tp->to_string() + " at lside");
    }
    tp = r->inner();
    cval = cval->get<sconst::ConstRef>().inner;
  } else {
    _recorder->tagged_report(kErrTag, "Dereferencing a not-reference type");
    return;
  }
  node.set_cval(cval);
}

void ConstEvaluator::postVisit(NegationExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = node.expr()->cval();
  auto primitive = inner->get_if<sconst::ConstPrime>();
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
      auto prime = inner->type().get<stype::PrimeType>()->prime();
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
      node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
        inner->type(), // still primitive
        prime,
        -arg // negation
      ));
    } else if constexpr(std::is_same_v<T, bool>) {
      if(node.oper() != Operator::kLogicalNot) {
        _recorder->tagged_report(kErrTag, "Invalid operator kLogicalNot in negation expression");
      }
      node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
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
  auto inner1 = node.expr1()->cval();
  auto inner2 = node.expr2()->cval();
  auto prime1 = inner1->get_if<sconst::ConstPrime>();
  auto prime2 = inner2->get_if<sconst::ConstPrime>();
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
            node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
              inner1->type(), arg1 << arg2));
          } else {
            node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
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
        auto prime1 = inner1->type().get<stype::PrimeType>()->prime();
        switch(oper) {
        case Operator::kAdd:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 + arg2));
          break;
        case Operator::kSub:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 - arg2));
          break;
        case Operator::kMul:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 * arg2));
          break;
        case Operator::kDiv:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 / arg2));
          break;
        case Operator::kMod: {
          T val = 0;
          if constexpr(std::is_floating_point_v<T>) {
            val = std::fmod(arg1, arg2);
          } else {
            val = arg1 % arg2;
          }
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, val));
        } break;
        case Operator::kBitwiseAnd:
        case Operator::kBitwiseOr:
        case Operator::kBitwiseXor:
          if constexpr(std::is_integral_v<T>) {
            if(oper == Operator::kBitwiseAnd)
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 & arg2));
            else if(oper == Operator::kBitwiseOr)
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 | arg2));
            else
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), prime1, arg1 ^ arg2));
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
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), stype::TypePrime::kBool, arg1 && arg2));
        } else if(oper == Operator::kLogicalOr) {
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(inner1->type(), stype::TypePrime::kBool, arg1 || arg2));
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
  auto inner1 = node.expr1()->cval();
  auto inner2 = node.expr2()->cval();
  auto prime1 = inner1->get_if<sconst::ConstPrime>();
  auto prime2 = inner2->get_if<sconst::ConstPrime>();
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
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool),
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
  auto inner1 = node.expr1()->cval();
  auto prime1 = inner1->get_if<sconst::ConstPrime>();
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
    node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
      _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool),
      stype::TypePrime::kBool,
      false));
    return;
  }
  if(oper == Operator::kLogicalOr && *result1) {
    node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
      _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool),
      stype::TypePrime::kBool,
      true));
    return;
  }

  node.expr2()->accept(*this);
  if(!node.expr2()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner expression const evaluation failed");
    return;
  }
  auto inner2 = node.expr2()->cval();
  auto prime2 = inner2->get_if<sconst::ConstPrime>();
  if(!prime2) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in boolean expression");
    return;
  }
  auto result2 = std::get_if<bool>(&prime2->value);
  if(!result2) {
    _recorder->tagged_report(kErrTag, "Inner type not boolean in boolean expression");
    return;
  }

  node.set_cval(inner2);
}

void ConstEvaluator::postVisit(TypeCastExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = node.expr()->cval();
  auto primitive = inner->get_if<sconst::ConstPrime>();
  if(!primitive) {
    _recorder->tagged_report(kErrTag, "Inner type not primitive in type cast expression");
    return;
  }

  auto target_type = node.type_no_bounds()->get_type().get_if<stype::PrimeType>();
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
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(TypePrime::kBool),
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
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(target_prime),
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
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(target_prime),
          target_prime,
          val
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to unsigned integer");
      }
    } break;


    case TypePrime::kF32: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(target_prime),
          target_prime,
          static_cast<float>(arg)
        ));
      } else {
        _recorder->tagged_report(kErrTag, "Cannot cast value to floating point");
      }
    } break;
    case TypePrime::kF64: {
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        node.set_cval(_const_pool->make_const<sconst::ConstPrime>(
          _type_pool->make_type<stype::PrimeType>(target_prime),
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
  node.set_cval(node.expr()->cval());
}

/********************* PreTypeFiller ***************************/

const std::string PreTypeFiller::kErrTypeNotResolved = "Error: Type not resolved";

void PreTypeFiller::postVisit(TypePath &node) {
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

void PreTypeFiller::postVisit(Function &node) {
  ScopedVisitor::postVisit(node); // exit the inner scope first. Add the symbol to the outer scope.
  std::vector<stype::TypePtr> params;
  if(node.params_opt()) {
    if(node.params_opt()->self_param_opt()) {
      _recorder->tagged_report(kErrTypeNotResolved, "Self param unimplemented...");
      return;
    }
    for(auto &param: node.params_opt()->func_params()) {
      if(param->has_name()) {
        auto ptr = static_cast<FunctionParamPattern *>(param.get());
        auto tp = ptr->type()->get_type();
        if(!tp) {
          _recorder->tagged_report(kErrTypeNotResolved, "Function parameter pattern type tag not resolved");
          return;
        }
        params.push_back(tp);
      } else {
        auto ptr = static_cast<FunctionParamType *>(param.get());
        auto tp = ptr->type()->get_type();
        if(!tp) {
          _recorder->tagged_report(kErrTypeNotResolved, "Function parameter type tag not resolved");
          return;
        }
        params.push_back(tp);
      }
    }
  }
  stype::TypePtr ret_type = node.res_type_opt() ? node.res_type_opt()->get_type() : _type_pool->make_unit();
  if(!ret_type)  {
    _recorder->tagged_report(kErrTypeNotResolved, "Function result type tag not resolved");
    return;
  }
  auto func_type = _type_pool->make_type<stype::FunctionType>(node.ident(), std::move(params), ret_type);
  add_symbol(node.ident(), SymbolInfo{
    .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction, .type = func_type
  });
  node.set_type(func_type); // ...
}


/********************** TypeFiller *****************************/

const std::string TypeFiller::kErrTypeNotResolved = "Error: Type not resolved";
const std::string TypeFiller::kErrTypeNotMatch = "Error: Type not match between evaluation and declaration";
const std::string TypeFiller::kErrConstevalFailed = "Error: Necessary const evaluation failed";
const std::string TypeFiller::kErrIdentNotResolved = "Error: Identifier not resolved as expected";
const std::string TypeFiller::kErrNoPlaceMutability = "Error: Place mutability required not exist";

void TypeFiller::postVisit(Function &node) {
  ScopedVisitor::postVisit(node); // exit the function scope first

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
      if(s->is_ref()) pt = _type_pool->make_type<stype::RefType>(pt, s->is_mut());
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
  type = _type_pool->make_type<stype::RefType>(type, node.is_mut());
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
  auto cval = node.const_expr()->cval()->get_if<sconst::ConstPrime>();
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
  // not needed.
}

void TypeFiller::postVisit(LiteralExpression &node) {
  node.set_type(_type_pool->make_type<stype::PrimeType>(node.prime()));
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
    if(info->kind == SymbolKind::kVariable && info->is_place_mut) node.set_place_mut();
    node.set_type(info->type);
  } else {
    _recorder->tagged_report(kErrIdentNotResolved, "Invalid type associated with the identifier");
    return;
  }
}

void TypeFiller::postVisit(BorrowExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in BorrowExpression");
    return;
  }
  type = _type_pool->make_type<stype::RefType>(type, node.is_mut());
  node.set_type(type);
}

void TypeFiller::postVisit(DereferenceExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in DereferenceExpression");
    return;
  }
  if(auto inner = type.get_if<stype::RefType>()) {
    if(inner->ref_is_mut()) node.set_place_mut();
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in NegationExpression");
    return;
  }
  node.set_type(std::move(type));
}

void TypeFiller::postVisit(ArithmeticOrLogicalExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in ArithmeticOrLogicalExpression");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in ArithmeticOrLogicalExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kShl:
  case Operator::kShr:
    if(prime1->is_integer() && prime2->is_unsigned_integer()) {
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in ComparisonExpression");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in ComparisonExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
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
      node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool));
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in LazyBooleanExpression");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in LazyBooleanExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
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
    _recorder->tagged_report(kErrTypeNotResolved, "From type not got in TypeCastExpression");
    return;
  }
  auto to_type = node.type_no_bounds()->get_type();
  if(!to_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "To type not got in TypeCastExpression");
    return;
  }
  auto from_prime = from_type.get_if<stype::PrimeType>();
  auto to_prime = to_type.get_if<stype::PrimeType>();
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

void TypeFiller::preVisit(AssignmentExpression &node) {
  node.expr1()->set_lside();
}

void TypeFiller::postVisit(AssignmentExpression &node) {
  if(!node.expr1()->is_place_mut()) {
    _recorder->tagged_report(kErrNoPlaceMutability, "To type not a place mutable type");
    return;
  }
  const auto to_type = node.expr1()->get_type();
  if(!to_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "To type not got in AssignmentExpression");
    return;
  }
  const auto from_type = node.expr2()->get_type();
  if(!from_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "From type not got in AssignmentExpression");
    return;
  }
  auto t = to_type, f = from_type;
  // Accepted assignment: (rt <- rf)
  // mut T <- T
  // &T <- &T
  // &T <- &mut T
  // &mut T <- &mut T
  // (auto deref acceptable) &mut T <- T
  bool to_accept = false;
  if(node.expr1()->is_place_mut() && t == f) to_accept = true;
  auto rt = t.get_if<stype::RefType>(), rf = f.get_if<stype::RefType>();
  if(rt && rf && rt->inner() == rf->inner() && (!rt->ref_is_mut() || rf->ref_is_mut()))
    to_accept = true;
  if(node.expr1()->allow_auto_deref() && rt && !rf && rt->ref_is_mut() && rt->inner() == f)
    to_accept = true; // ...
  if(!to_accept) {
    _recorder->tagged_report(kErrTypeNotMatch, "Invalid assignment: "
      + to_type->to_string() + " <- " + from_type->to_string());
    return;
  }
  node.set_type(to_type);
}

void TypeFiller::preVisit(CompoundAssignmentExpression &node) {
  node.expr1()->set_lside();
}

void TypeFiller::visit(Function &node) {
  ScopedVisitor::preVisit(node); // enter the inner scope
  if(node.params_opt()) node.params_opt()->accept(*this);
  if(node.res_type_opt()) node.res_type_opt()->accept(*this);

  // register the parameter names
  if(node.params_opt()) {
    // ignore self param

    if(node.params_opt()) for(auto &param: node.params_opt()->func_params()) {
      if(param->has_name()) {
        auto ptr = static_cast<FunctionParamPattern*>(param.get());
        bind_pattern(ptr->pattern().get(), ptr->type()->get_type());
      }
    }
  }

  if(node.body_opt()) node.body_opt()->accept(*this);
  ScopedVisitor::postVisit(node);
}


void TypeFiller::postVisit(CompoundAssignmentExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in CompoundAssignmentExpression");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in CompoundAssignmentExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kShlAssign:
  case Operator::kShrAssign:
    if(prime1->is_integer() && prime2->is_unsigned_integer()) {
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in GroupedExpression");
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
        _recorder->tagged_report(kErrTypeNotResolved, "Type not got in ArrayExpression");
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
    auto length_prime = arr->len_expr()->cval()->get_if<sconst::ConstPrime>();
    if(!length_prime) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not a primitive");
      return;
    }
    auto length = length_prime->get_usize();
    if(!length) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not an unsigned value");
      return;
    }
    auto type = arr->val_expr()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Expr type not got in ArrayExpression");
      return;
    }
    node.set_type(_type_pool->make_type<stype::ArrayType>(type, *length));
  }
}

void TypeFiller::postVisit(IndexExpression &node) {
  auto expr_type = node.expr_obj()->get_type();
  if(!expr_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Expr type not got in IndexExpression");
    return;
  }
  auto index_type = node.expr_index()->get_type();
  if(!index_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Index type not got in IndexExpression");
    return;
  }

  // out_of_range is a runtime problem
  auto index_primitive = index_type.get_if<stype::PrimeType>();
  if(!index_primitive || !index_primitive->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Index not an integer");
    return;
  }

  // (mut) [T; N] -> &(mut) T
  // (mut) &[T; N] -> &T
  // (mut) &mut [T; N] -> &T (rside), &mut T (lside)

  if(auto a = expr_type.get_if<stype::ArrayType>()) {
    auto tp = a->inner();
    bool is_mut = node.expr_obj()->is_place_mut();
    tp = _type_pool->make_type<stype::RefType>(tp, is_mut);
    node.set_type(tp);
    if(is_mut) node.set_place_mut();
    return;
  }
  if(auto r = expr_type.get_if<stype::RefType>()) {
    auto a = r->inner().get_if<stype::ArrayType>();
    if(!a) {
      _recorder->tagged_report(kErrTypeNotMatch, "Indexing target not an array or array reference: "
        + expr_type->to_string());
      return;
    }
    auto tp = a->inner();
    bool is_mut = r->ref_is_mut() && node.is_lside();
    tp = _type_pool->make_type<stype::RefType>(tp, is_mut);
    node.set_type(tp);
    if(is_mut) node.set_place_mut();
    return;
  }
  _recorder->tagged_report(kErrTypeNotMatch, "Indexing target not an array or array reference: "
    + expr_type->to_string());
}

void TypeFiller::postVisit(TupleExpression &node) {
  std::vector<stype::TypePtr> members;
  if(node.elems_opt()) for(const auto &expr: node.elems_opt()->expr_list()) {
    auto t = expr->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got in TupleExpression");
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
  node.set_type(func_type->ret_type());
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
  node.set_type(_type_pool->make_never());
  node.set_always_returns();
}

void TypeFiller::postVisit(BreakExpression &node) {
  // set never type
  node.set_type(_type_pool->make_never());
  node.set_always_returns();
}

void TypeFiller::postVisit(RangeExpr &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in RangeExpr");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in RangeExpr");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in RangeFromExpr");
    return;
  }
  auto prime = type.get_if<stype::PrimeType>();
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in RangeToExpr");
    return;
  }
  auto prime = type.get_if<stype::PrimeType>();
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in RangeInclusiveExpr");
    return;
  }
  auto type2 = node.expr1()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in RangeInclusiveExpr");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got in RangeToInclusiveExpr");
    return;
  }
  auto prime = type.get_if<stype::PrimeType>();
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
  // set never type as result
  node.set_type(_type_pool->make_never());
  node.set_always_returns();
}

void TypeFiller::postVisit(UnderscoreExpression &node) {
  throw std::runtime_error("UnderscoreExpression not supported");
  return;
}

void TypeFiller::postVisit(BlockExpression &node) {
  // Rule:
  // If there is a final expr_opt, use its type.
  // Or, check the return / break / continue.
  // If there's a never-type, terminate and set the block as never-type.
  if(!node.stmts_opt()) {
    node.set_type(_type_pool->make_unit());
    return;
  }
  bool always_returns = false;
  for(auto &stmt: node.stmts_opt()->stmts()) {
    auto ptr = dynamic_cast<ExpressionStatement*>(stmt.get());
    if(!ptr) continue;
    if(ptr->expr()->always_returns()) {
      always_returns = true;
      break;
    }
  }
  stype::TypePtr t = _type_pool->make_unit();
  if(node.stmts_opt()->expr_opt()) {
    t = node.stmts_opt()->expr_opt()->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Expr stmt exists, but type not got");
      return;
    }
    if(node.stmts_opt()->expr_opt()->always_returns())
      always_returns = true;
  }
  if(always_returns) {
    t = _type_pool->make_never();
    node.set_always_returns();
  }
  node.set_type(t);
}

void TypeFiller::postVisit(FunctionBodyExpr &node) {
  stype::TypePtr rt;
  if(!node.func_returns().empty()) {
    // check whether these return types cooperate
    auto &opt = node.func_returns().front()->expr_opt();
    auto t = opt ? opt->get_type() : _type_pool->make_unit();
    if(!t) {
      _recorder->tagged_report(kErrIdentNotResolved, "Return type not resolved");
      return;
    }
    for(auto &elem: node.func_returns()) {
      auto &opt2 = elem->expr_opt();
      auto t2 = opt2 ? opt2->get_type() : _type_pool->make_unit();
      if(!t2) {
        _recorder->tagged_report(kErrIdentNotResolved, "Return type not resolved");
        return;
      }
      if(t != t2) {
        _recorder->tagged_report(kErrTypeNotMatch, "FunctionBodyExpression has different return type,"
          " " + t->to_string() + " vs " + t2->to_string());
        return;
      }
    }
    rt = t;
  }

  stype::TypePtr st;
  if(!node.stmts_opt()) {
    st = _type_pool->make_unit();
  } else {
    bool always_returns = false;
    for(auto &stmt: node.stmts_opt()->stmts()) {
      auto ptr = dynamic_cast<ExpressionStatement*>(stmt.get());
      if(!ptr) continue;
      if(ptr->expr()->always_returns()) {
        always_returns = true;
        break;
      }
    }
    st = _type_pool->make_unit();
    if(node.stmts_opt()->expr_opt()) {
      st = node.stmts_opt()->expr_opt()->get_type();
      if(!st) {
        _recorder->tagged_report(kErrTypeNotResolved, "Expr stmt exists, but type not got");
        return;
      }
      if(node.stmts_opt()->expr_opt()->always_returns())
        always_returns = true;
    }
    if(always_returns) {
      st = _type_pool->make_never();
      node.set_always_returns();
    }
  }

  if(!rt) {
    // no return expr.
    node.set_type(st);
  } else {
    // st == never: see rt.
    // st != never: st shall be equal to rt.
    if(st == _type_pool->make_never()) {
      node.set_type(rt);
    } else if(st != rt) {
      _recorder->tagged_report(kErrTypeNotMatch, "Func tail expr type not the same with return type");
      return;
    } else {
      node.set_type(st);
    }
  }
}


void TypeFiller::postVisit(InfiniteLoopExpression &node) {
  // loop {...}
  if(node.loop_breaks().empty()) {
    node.set_type(_type_pool->make_type<stype::NeverType>());
    return;
  }
  auto type = node.loop_breaks().front()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Result type not got in InfiniteLoopExpression");
    return;
  }
  for(const auto &elem: node.loop_breaks()) {
    // compared first element again... never mind
    auto t = elem->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Result type not got in InfiniteLoopExpression");
      return;
    }
    if(t != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "InfiniteLoopExpression has different return type");
      return;
    }
  }
  if(node.block_expr()->always_returns())
    node.set_always_returns();
  node.set_type(type);
}

void TypeFiller::postVisit(PredicateLoopExpression &node) {
  // while ... {}
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimeType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }

  auto type = _type_pool->make_unit();
  if(node.block_expr()->stmts_opt() && node.block_expr()->stmts_opt()->expr_opt()) {
    type = node.block_expr()->stmts_opt()->expr_opt()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Result type not got in PredicateLoopExpression");
      return;
    }
  }
  for(const auto &elem: node.loop_breaks()) {
    // compared first element again... never mind
    auto t = elem->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Result type not got in PredicateLoopExpression");
      return;
    }
    if(t != type) {
      _recorder->tagged_report(kErrTypeNotMatch, "PredicateLoopExpression has different return type");
      return;
    }
  }

  if(node.cond()->expr()->always_returns() || node.block_expr()->always_returns())
    node.set_always_returns();
  node.set_type(type);
}

void TypeFiller::postVisit(IfExpression &node) {
  // if ... {} else ...
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimeType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }

  bool always_returns = true;
  if(!node.block_expr()->always_returns()) always_returns = false;

  auto type = node.block_expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Result type not got in IfExpression");
    return;
  }
  bool success = true;
  std::visit([&]<typename T0>(T0 &&arg) {
    using T = std::decay_t<T0>;
    if constexpr(!std::is_same_v<T, std::monostate>) {
      // T = std::unique_ptr<BlockExpression/IfExpression>
      auto t = arg->get_type();
      if(!t) success = false;
      if(type == _type_pool->make_never()) {
        type = t;
      } else if(t != _type_pool->make_never() && t != type) {
        success = false;
      }
      if(!arg->always_returns()) always_returns = false;
    } else {
      always_returns = false; // no else/elif, impossible to be always returning
    }
  }, node.else_spec());
  if(!success) {
    _recorder->tagged_report(kErrTypeNotMatch, "Type not match between if blocks");
    return;
  }

  if(node.cond()->expr()->always_returns()) always_returns = true;

  node.set_type(type);
  if(always_returns) node.set_always_returns();
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

  // r: R
  // let mut x = r, x: mut R
  // let ref y = r, y: &R
  // let ref mut z = r, z: &mut R
  bool is_place_mut = false;
  if(pattern->is_ref()) {
    type = _type_pool->make_type<stype::RefType>(type, pattern->is_mut());
  } else if(pattern->is_mut()) {
    is_place_mut = true;
  }
  auto info = add_symbol(pattern->ident(), SymbolInfo{
    .node = pattern, .ident = pattern->ident(), .kind = SymbolKind::kVariable,
    .type = type, .is_place_mut = is_place_mut
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
  if(auto r = type.get_if<stype::RefType>(); !r) {
    _recorder->report("Type binding failed: not a reference");
    return;
  } else if(pattern->is_mut() ^ r->ref_is_mut()) {
    _recorder->report("Type binding failed: reference mutability mismatch");
    return;
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