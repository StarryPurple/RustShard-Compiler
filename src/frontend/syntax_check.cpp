#include "syntax_check.h"

#include <limits>
#include <cmath>

namespace insomnia::rust_shard::ast {

/*************************** ConstEvaluator ***************************************/

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
  auto inner = node.expr()->const_value();
  node.set_const_value(_const_pool->make_const<sem_const::ConstReference>(
    _type_pool->make_type<sem_type::ReferenceType>(inner->type(), node.is_mut()),
    inner
  ));
}

void ConstEvaluator::postVisit(DereferenceExpression &node) {
  if(!node.expr()->has_constant()) {
    _recorder->tagged_report(kErrTag, "Inner const evaluation failed");
    return;
  }
  auto inner = node.expr()->const_value();
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
  auto inner = node.expr()->const_value();
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
  auto inner1 = node.expr1()->const_value();
  auto inner2 = node.expr2()->const_value();
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
        switch(oper) {
        case Operator::kAdd:
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 + arg2));
          break;
        case Operator::kSub:
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 - arg2));
          break;
        case Operator::kMul:
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 * arg2));
          break;
        case Operator::kDiv:
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), arg1 / arg2));
          break;
        case Operator::kMod: {
          T val = 0;
          if constexpr(std::is_floating_point_v<T>) {
            val = std::fmod(arg1, arg2);
          } else {
            val = arg1 % arg2;
          }
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(inner1->type(), val));
        } break;
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
          node.set_const_value(_const_pool->make_const<sem_const::ConstPrimitive>(
            _type_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kBool),
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
  auto inner2 = node.expr2()->const_value();
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
  auto inner = node.expr()->const_value();
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
  node.set_const_value(node.expr()->const_value());
}

/********************** TypeFiller *****************************/

const std::string TypeFiller::kErrTypeNotResolved = "Error: Type not resolved";
const std::string TypeFiller::kErrTypeNotMatch = "Error: Type not match between evaluation and declaration";
const std::string TypeFiller::kErrConstevalFailed = "Error: Necessary const evaluation failed";

void TypeFiller::postVisit(Function &node) {
  // Function not checked
  return;
}

void TypeFiller::postVisit(StructStruct &node) {
  auto info = find_symbol(node.ident());
  std::map<std::string_view, sem_type::TypePtr> struct_fields;
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
  info->type.get<sem_type::StructType>()->set_fields(std::move(struct_fields));
}

void TypeFiller::postVisit(Enumeration &node) {
  auto info = find_symbol(node.ident());
  auto raw_enum_type = info->type.get_if<sem_type::EnumType>();
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
  throw std::runtime_error("Trait not implemented");
  return;
}

void TypeFiller::postVisit(TypeAlias &node) {
  auto info = find_symbol(node.ident());
  auto alias_type = info->type.get_if<sem_type::AliasType>();
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
  std::vector<sem_type::TypePtr> types;
  for(const auto &item: node.types()) {
    auto type = item->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside tuple");
      return;
    }
    types.push_back(type);
  }
  node.set_type(_type_pool->make_type<sem_type::TupleType>(std::move(types)));
}

void TypeFiller::postVisit(ReferenceType &node) {
  auto type = node.type_no_bounds()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside reference chain");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::ReferenceType>(std::move(type), node.is_mut()));
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
  auto cval = node.const_expr()->const_value()->get_if<sem_const::ConstPrimitive>();
  if(!cval) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not a primitive type");
    return;
  }
  auto length = std::get_if<sem_type::index_t>(&cval->value);
  if(!length) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not an index value");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::ArrayType>(std::move(type), *length));
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
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"Self\" is not supported yet");
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
      auto enum_type = info->type.get_if<sem_type::EnumType>();
      if(!enum_type) {
        throw std::runtime_error("Fatal error: enum type mismatch undetected");
      }
      if(auto vit = enum_type->variants().find(item_ident); vit == enum_type->variants().end()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Not a valid enum variant");
        return;
      } else if(!vit->second) {
        throw std::runtime_error("Fatal error: enum variant type not set");
      } else {
        node.set_type(sem_type::TypePtr(vit->second));
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
  node.set_type(_type_pool->make_type<sem_type::PrimitiveType>(node.prime()));
}

void TypeFiller::postVisit(PathInExpression &node) {
  // unsupported
  throw std::runtime_error("PathInExpression not implemented yet");
}

void TypeFiller::postVisit(BorrowExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::ReferenceType>(std::move(type), node.is_mut()));
}

void TypeFiller::postVisit(DereferenceExpression &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto sub_type = type.get_if<sem_type::ReferenceType>();
  if(!sub_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not reference");
    return;
  }
  node.set_type(sem_type::TypePtr(sub_type));
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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
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
    if(prime1->prime() == prime2->prime() && prime1->prime() == sem_type::TypePrime::kBool) {
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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
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
      node.set_type(_type_pool->make_type<sem_type::PrimitiveType>(sem_type::TypePrime::kBool));
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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(node.oper()) {
  case Operator::kLogicalAnd:
  case Operator::kLogicalNot:
    if(prime1->prime() == prime2->prime() && prime1->prime() == sem_type::TypePrime::kBool) {
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
  auto from_prime = from_type.get_if<sem_type::PrimitiveType>();
  auto to_prime = to_type.get_if<sem_type::PrimitiveType>();
  if(!from_prime || !to_prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  switch(to_prime->prime()) {
    using sem_type::TypePrime;

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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
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
    node.set_type(_type_pool->make_type<sem_type::ArrayType>(type, arr->expr_list().size()));
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
    auto length_prime = arr->len_expr()->const_value()->get_if<sem_const::ConstPrimitive>();
    if(!length_prime) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not a primitive");
      return;
    }
    auto length = std::get_if<sem_type::index_t>(&length_prime->value);
    if(!length) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not an unsigned value");
      return;
    }
    auto type = arr->val_expr()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    node.set_type(_type_pool->make_type<sem_type::ArrayType>(type, *length));
  }
}

void TypeFiller::postVisit(IndexExpression &node) {
  throw std::runtime_error("IndexExpression not supported");
  return;
}

void TypeFiller::postVisit(TupleExpression &node) {
  std::vector<sem_type::TypePtr> members;
  if(node.elems_opt()) for(const auto &expr: node.elems_opt()->expr_list()) {
    auto t = expr->get_type();
    if(!t) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
      return;
    }
    members.push_back(std::move(t));
  }
  node.set_type(_type_pool->make_type<sem_type::TupleType>(std::move(members)));
}

void TypeFiller::postVisit(TupleIndexingExpression &node) {
  throw std::runtime_error("TupleIndexExpression not supported");
  return;
}

void TypeFiller::postVisit(StructExpression &node) {
  throw std::runtime_error("StructExpression not supported");
  return;
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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime1->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeExpr");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::RangeType>(type1));
}

void TypeFiller::postVisit(RangeFromExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<sem_type::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeFromExpr");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::RangeType>(type));
}

void TypeFiller::postVisit(RangeToExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<sem_type::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeToExpr");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::RangeType>(type));
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
  auto prime1 = type1.get_if<sem_type::PrimitiveType>();
  auto prime2 = type2.get_if<sem_type::PrimitiveType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime1->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeInclusiveExpr");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::RangeType>(type1));
}

void TypeFiller::postVisit(RangeToInclusiveExpr &node) {
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not got");
    return;
  }
  auto prime = type.get_if<sem_type::PrimitiveType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive");
    return;
  }
  if(!prime->is_integer()) {
    _recorder->tagged_report(kErrTypeNotMatch, "Expected integer type in RangeToInclusiveExpr");
    return;
  }
  node.set_type(_type_pool->make_type<sem_type::RangeType>(type));
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

void TypeFiller::postVisit(InfiniteLoopExpression &node) {
  throw std::runtime_error("InfiniteLoopExpression not supported");
  return;
}

void TypeFiller::postVisit(PredicateLoopExpression &node) {
  throw std::runtime_error("PredicateLoopExpression not supported");
  return;
}

void TypeFiller::postVisit(IfExpression &node) {
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean");
    return;
  }
  auto cond_prime = cond_type.get_if<sem_type::PrimitiveType>();
  if(!cond_prime || cond_prime->prime() != sem_type::TypePrime::kBool) {
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
  if(node.type_opt() && node.type_opt()->get_type() != type) {
    _recorder->tagged_report(kErrTypeNotMatch, "type mismatch inside let statement");
    return;
  }
  bind_pattern(node.pattern().get(), type);
}

void TypeFiller::bind_pattern(PatternNoTopAlt *pattern, sem_type::TypePtr type) {
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

void TypeFiller::bind_identifier(IdentifierPattern *pattern, sem_type::TypePtr type) {
  // always success
  bool is_mut = pattern->is_mut();
  if(pattern->is_ref()) {
    // let ref y = r <=> let y = &r
    // let ref mut z = r <=> let z = &mut r
    type = _type_pool->make_type<sem_type::ReferenceType>(type, is_mut);
    is_mut = false; // z is immutable (while *z is mutable)
  }
  auto info = add_symbol(pattern->ident(), SymbolInfo{
    .node = pattern, .ident = pattern->ident(), .kind = SymbolKind::kVariable,
    .is_mut = is_mut, .type = type
  });
  if(!info) {
    _recorder->report("Variable identifier already defined");
    return;
  }
}

void TypeFiller::bind_wildcard(WildcardPattern *pattern, sem_type::TypePtr type) {
  // always success
  // nothing to do here
}

void TypeFiller::bind_tuple(TuplePattern *pattern, sem_type::TypePtr type) {
  // let (a, b) = (1, 2) -> a = 1, b = 2
  auto t = type.get_if<sem_type::TupleType>();
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

void TypeFiller::bind_struct(StructPattern *pattern, sem_type::TypePtr type) {
  // let Type { x1: x, y1: y } = Type{ x1: 1, y1: 2, z1: 3 } -> x = 1, y = 2
  auto t = type.get_if<sem_type::StructType>();
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

void TypeFiller::bind_reference(ReferencePattern *pattern, sem_type::TypePtr type) {
  auto t = type.get_if<sem_type::ReferenceType>();
  if(!t) {
    _recorder->report("Type binding failed: not a reference");
    return;
  }
  if(t->is_mut() != pattern->is_mut()) {
    _recorder->report("Type binding failed: reference mutability not match");
    return;
  }
  auto sub_pattern = pattern->pattern().get();
  bind_pattern(sub_pattern, t->type());
}

void TypeFiller::bind_literal(LiteralPattern *pattern, sem_type::TypePtr type) {
  _recorder->report("Type binding error: literal pattern not supported in let statement");
}

void TypeFiller::bind_grouped(GroupedPattern *pattern, sem_type::TypePtr type) {
  auto sub_pattern = pattern->pattern().get();
  if(sub_pattern->patterns().size() != 1) {
    _recorder->report("Type binding failed: multi pattern not supported yet");
    return;
  }
  bind_pattern(sub_pattern->patterns().front().get(), type);
}

void TypeFiller::bind_slice(SlicePattern *pattern, sem_type::TypePtr type) {
  _recorder->report("Slice Not supported yet");
}

void TypeFiller::bind_path(PathPattern *pattern, sem_type::TypePtr type) {
  _recorder->report("Type binding error: path pattern not supported in let statement");
}

}





























