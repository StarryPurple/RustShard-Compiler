#include "syntax_check.h"

#include <limits>
#include <cmath>

namespace insomnia::rust_shard::ast {

/*************************** TypeDeclarator ***************************************/

void TypeDeclarator::load_builtin(Crate *crate) {
  using namespace stype;

  // primitive types
  for(const auto prime: type_primes()) {
    auto ident = prime_strs(prime);
    crate->scope()->add_symbol(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kPrimitiveType,
      .type = _type_pool->make_type<PrimeType>(prime)
    });
  }

  // builtin functions
  std::vector<std::pair<StringRef, TypePtr>> builtin_functions = {{
      // fn print(s: &str) -> ()
      "print", _type_pool->make_type<FunctionType>(
        "print",
        TypePtr{},
        std::vector{
          _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
      },
      _type_pool->make_unit())
    }, {
      // fn println(s: &str) -> ()
      "println", _type_pool->make_type<FunctionType>(
        "println",
        TypePtr{},
        std::vector{
          _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
      },
      _type_pool->make_unit()
      )
    }, {
      // fn printInt(n: i32) -> ()
      "printInt", _type_pool->make_type<FunctionType>(
        "printInt",
        TypePtr{},
        std::vector{
          _type_pool->make_type<PrimeType>(TypePrime::kI32),
      },
      _type_pool->make_unit()
      )
    }, {
      // fn printlnInt(n: i32) -> ()
      "printlnInt", _type_pool->make_type<FunctionType>(
        "printlnInt",
        TypePtr{},
        std::vector{
          _type_pool->make_type<PrimeType>(TypePrime::kI32),
      },
      _type_pool->make_unit()
      )
    }, {
      // fn getString() -> String
      "getString", _type_pool->make_type<FunctionType>(
        "getString",
        TypePtr{},
        std::vector<TypePtr>{},
          _type_pool->make_type<PrimeType>(TypePrime::kStr)
      )
    }, {
      // fn getInt() -> i32
      "getInt", _type_pool->make_type<FunctionType>(
        "getInt",
        TypePtr{},
        std::vector<TypePtr>{},
          _type_pool->make_type<PrimeType>(TypePrime::kI32)
      )
    }, {
      // fn readInt() -> i32
      "readInt", _type_pool->make_type<FunctionType>(
        "readInt",
        TypePtr{},
        std::vector<TypePtr>{},
          _type_pool->make_type<PrimeType>(TypePrime::kI32)
      )
    }, {
      // fn exit(code: i32) -> ()
      "exit", _type_pool->make_type<FunctionType>(
        "exit",
        TypePtr{},
        std::vector{
          _type_pool->make_type<PrimeType>(TypePrime::kI32),
      },
      _type_pool->make_unit()
      )
    }, {
      // fn from(&str) -> String
      // fn from(&mut str) -> String
      // (As I didn't implement function signature mechanic, I'll ignore the strictly stricter version)
      "from", _type_pool->make_type<FunctionType>(
        "from",
        TypePtr{},
        std::vector{
          _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
      },
      _type_pool->make_type<PrimeType>(TypePrime::kString)
      )
    },
  };
  for(const auto &[ident, type]: builtin_functions) {
    crate->scope()->add_symbol(ident, SymbolInfo{
      .node = nullptr,
      .ident = ident,
      .kind = SymbolKind::kFunction,
      .type = type
    });
  }

  // builtin methods
  std::vector<std::pair<TypePtr, std::shared_ptr<FunctionType>>> builtin_methods = {
    {
      // impl String
      // fn as_str(&self) -> &str
      _type_pool->make_type<PrimeType>(TypePrime::kString), _type_pool->make_raw_type<FunctionType>(
        "as_str",
        _type_pool->make_type<PrimeType>(TypePrime::kString),
        std::vector<TypePtr>{},
        _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false)
      )
    }, {
      // impl String
      // fn as_mut_str(&mut self) -> &mut str
      _type_pool->make_type<PrimeType>(TypePrime::kString), _type_pool->make_raw_type<FunctionType>(
        "as_mut_str",
        _type_pool->make_type<PrimeType>(TypePrime::kString),
        std::vector<TypePtr>{},
        _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), true)
      )
    }, {
      // impl String
      // fn append(&mut self, s: &str) -> ()
      _type_pool->make_type<PrimeType>(TypePrime::kString), _type_pool->make_raw_type<FunctionType>(
        "append",
        _type_pool->make_type<PrimeType>(TypePrime::kString),
        std::vector{
          _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false)
        },
        _type_pool->make_unit()
      )
    }
  };
  // impl #primitive types#
  // fn to_string(&self) -> String
  for(const auto prime: type_primes()) {
    builtin_methods.emplace_back(
      _type_pool->make_type<PrimeType>(prime), _type_pool->make_raw_type<FunctionType>(
        "to_string",
        _type_pool->make_type<PrimeType>(prime),
        std::vector<TypePtr>(),
        _type_pool->make_type<PrimeType>(TypePrime::kString)
        )
      );
  }
  // impl String, &str, &mut str
  // fn len(&self) -> usize / fn length(&self) -> usize
  builtin_methods.emplace_back(
    _type_pool->make_type<PrimeType>(TypePrime::kString), _type_pool->make_raw_type<FunctionType>(
      "len",
      _type_pool->make_type<PrimeType>(TypePrime::kString),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
    );
  builtin_methods.emplace_back(
    _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
    _type_pool->make_raw_type<FunctionType>(
      "len",
      _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
    );
  builtin_methods.emplace_back(
    _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), true),
    _type_pool->make_raw_type<FunctionType>(
      "len",
      _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), true),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
      );
  builtin_methods.emplace_back(
    _type_pool->make_type<PrimeType>(TypePrime::kString), _type_pool->make_raw_type<FunctionType>(
      "length",
      _type_pool->make_type<PrimeType>(TypePrime::kString),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
    );
  builtin_methods.emplace_back(
    _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
    _type_pool->make_raw_type<FunctionType>(
      "length",
      _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), false),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
    );
  builtin_methods.emplace_back(
    _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), true),
    _type_pool->make_raw_type<FunctionType>(
      "length",
      _type_pool->make_type<RefType>(_type_pool->make_type<PrimeType>(TypePrime::kStr), true),
      std::vector<TypePtr>{},
      _type_pool->make_type<PrimeType>(TypePrime::kUSize)
      )
    );
  // impl<T, N> [T; N]
  // fn len(&self) -> usize / fn length(&self) -> usize
  // note: builtin in Crate::find_asso_item (or something alike)
  // is dynamically added while called.

  for(const auto &[caller_type, func_type]: builtin_methods) {
    crate->add_asso_method(caller_type, func_type);
  }
}

/*************************** ConstEvaluator ***************************************/

void ConstEvaluator::preVisit(AssignmentExpression &node) {
  node.expr1()->set_lside();
}

void ConstEvaluator::preVisit(CompoundAssignmentExpression &node) {
  node.expr1()->set_lside();
}

void ConstEvaluator::postVisit(LiteralExpression &node) {
  auto prime = node.prime();
  stype::TypePtr type = _type_pool->make_type<stype::PrimeType>(prime);
  if(prime != stype::TypePrime::kStr) {
    std::visit([&](auto &&arg) {
      node.set_cval(
        _const_pool->make_const<sconst::ConstPrime>(type, prime, arg)
      );
    }, node.spec_value());
  } else {
    auto ref = _type_pool->make_type<stype::RefType>(type, false);
    std::visit([&](auto &&arg) {
      node.set_cval(
        _const_pool->make_const<sconst::ConstRef>(
          ref,
          _const_pool->make_const<sconst::ConstPrime>(type, prime, arg),
          false)
      );
    }, node.spec_value());
  }
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

  auto tp1 = inner1->type(), tp2 = inner2->type();
  if(!tp1->is_convertible_from(*tp2) && !tp2->is_convertible_from(*tp1)) {
    _recorder->tagged_report(kErrTag, "Mismatched types in arithmetic/logical expression");
    return;
  }

  // TODO: NaN implementation (currently treated as compilation error)

  std::visit([&]<typename T01, typename T02>(T01 &&arg1, T02 &&arg2) {
    using T1 = std::decay_t<T01>;
    using T2 = std::decay_t<T02>;
    auto tp12 = tp1->is_convertible_from(*tp2) ? tp1 : tp2;
    if constexpr(!std::is_same_v<T1, T2>) {
      throw std::runtime_error("Type mismatch not checked, inner design has flaws.");
    } else {
      using T = T1;
      if constexpr(type_utils::is_one_of<T, std::int64_t, std::uint64_t, float, double>) {
        if((oper == Operator::kDiv || oper == Operator::kMod) && arg2 == 0) {
          _recorder->tagged_report(kErrTag, "Division by zero");
          return;
        }
        auto prime12 = tp12.get<stype::PrimeType>()->prime();
        switch(oper) {
        case Operator::kAdd:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 + arg2));
          break;
        case Operator::kSub:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 - arg2));
          break;
        case Operator::kMul:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 * arg2));
          break;
        case Operator::kDiv:
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 / arg2));
          break;
        case Operator::kMod: {
          T val = 0;
          if constexpr(std::is_floating_point_v<T>) {
            val = std::fmod(arg1, arg2);
          } else {
            val = arg1 % arg2;
          }
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, val));
        } break;
        case Operator::kBitwiseAnd:
        case Operator::kBitwiseOr:
        case Operator::kBitwiseXor:
          if constexpr(std::is_integral_v<T>) {
            if(oper == Operator::kBitwiseAnd)
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 & arg2));
            else if(oper == Operator::kBitwiseOr)
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 | arg2));
            else
              node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, prime12, arg1 ^ arg2));
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
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, stype::TypePrime::kBool, arg1 && arg2));
        } else if(oper == Operator::kLogicalOr) {
          node.set_cval(_const_pool->make_const<sconst::ConstPrime>(tp12, stype::TypePrime::kBool, arg1 || arg2));
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
  if(!inner1->type()->is_convertible_from(*inner2->type()) && !inner2->type()->is_convertible_from(*inner1->type())) {
    _recorder->tagged_report(kErrTag, "Mismatched types in comparison expression");
    return;
  }
  if(node.oper() == Operator::kGt || node.oper() == Operator::kLt
    || node.oper() == Operator::kGe || node.oper() == Operator::kGt) {
    auto prime1 = inner1->get_if<sconst::ConstPrime>();
    auto prime2 = inner2->get_if<sconst::ConstPrime>();
    if(!prime1 || !prime2) {
      _recorder->tagged_report(kErrTag, "Inner type not primitive in comparison expression");
      return;
    }
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
}, inner1->const_val(), inner2->const_val());
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

void ConstEvaluator::postVisit(PathInExpression &node) {
  if(node.segments().size() != 1) {
    _recorder->report("Unimplemented PathInExpression constant");
    return;
  }
  auto ident = node.segments().front()->ident_seg()->ident();
  auto info = find_symbol(ident);
  if(!info || info->kind != SymbolKind::kConstant) {
    _recorder->report("Unrecognized identifier as constant");
    return;
  }
  node.set_cval(info->cval);
}

void ConstEvaluator::postVisit(ArrayExpression &node) {
  stype::TypePtr tp;
  std::vector<sconst::ConstValPtr> vec;
  if(node.elements_opt()) {
    if(node.elements_opt()->is_explicit()) {
      auto ptr = static_cast<ExplicitArrayElements*>(node.elements_opt().get());
      bool is_first = true;
      for(auto &elem: ptr->expr_list()) {
        if(!elem->has_constant()) {
          _recorder->report("Array elements have no constant value");
          return;
        }
        if(!is_first && tp != elem->cval()->type()) {
          _recorder->report("Array elements have different types");
          return;
        }
        tp = elem->cval()->type();
        is_first = false;
        vec.push_back(elem->cval());
      }
    } else {
      auto ptr = static_cast<RepeatedArrayElements*>(node.elements_opt().get());
      if(!ptr->len_expr()->has_constant()) {
        _recorder->report("Repeated Array have no constant length");
        return;
      }
      if(!ptr->val_expr()->has_constant()) {
        _recorder->report("Repeated Array have no constant value");
        return;
      }
      auto cprime = ptr->val_expr()->cval()->get_if<sconst::ConstPrime>();
      if(!cprime) {
        _recorder->report("Repeated Array has a non-prime length");
        return;
      }
      auto length_opt = cprime->get_usize();
      if(!length_opt) {
        _recorder->report("Repeated Array has a non_usize length");
        return;
      }
      vec.resize(*length_opt, ptr->val_expr()->cval());
      tp = ptr->val_expr()->cval()->type();
    }
  }
  if(!tp) {
    _recorder->report("Unresolved type in Array Expression");
    return;
  }
  tp = _type_pool->make_type<stype::ArrayType>(tp, vec.size());
  auto cval = _const_pool->make_const<sconst::ConstArray>(
    tp,
    std::move(vec)
  );
  node.set_cval(cval);
}


/********************* PreTypeFiller ***************************/

const std::string PreTypeFiller::kErrTypeNotResolved = "Error: Type not resolved";
const std::string PreTypeFiller::kErrTypeInvalid = "Error: Invalid type";

void PreTypeFiller::postVisit(ParenthesizedType &node) {
  auto type = node.type()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside parenthesis");
    return;
  }
  node.set_type(type);
}

void PreTypeFiller::postVisit(TupleType &node) {
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

void PreTypeFiller::postVisit(ReferenceType &node) {
  auto type = node.type_no_bounds()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside reference chain");
    return;
  }
  type = _type_pool->make_type<stype::RefType>(type, node.is_mut());
  node.set_type(type);
}

void PreTypeFiller::postVisit(ArrayType &node) {
  auto type = node.type()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "unresolved variable type inside array");
    return;
  }
  _evaluator.constEvaluate(*node.const_expr(), _scopes);
  if(!node.const_expr()->has_constant()) {
    _recorder->tagged_report(kErrTypeNotResolved, "non-evaluable array length");
    return;
  }
  auto cval = node.const_expr()->cval()->get_if<sconst::ConstPrime>();
  if(!cval) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not a primitive type");
    return;
  }
  auto length_opt = cval->get_usize();
  if(!length_opt) {
    _recorder->tagged_report(kErrTypeNotResolved, "array length not an index value");
    return;
  }
  node.set_type(_type_pool->make_type<stype::ArrayType>(std::move(type), *length_opt));
}

void PreTypeFiller::postVisit(SliceType &node) {
  _recorder->tagged_report(kErrTypeNotResolved, "Slice not supported yet");
}

void PreTypeFiller::postVisit(TypePath &node) {
  // Enumeration/crate/generics
  // Since we only support single crate / no generic / simple enumeration,
  // we only have to check enumeration items.
  if(node.is_absolute()) {
    // ... I don't know.
  }
  for(int i = 0; i < node.segments().size(); ++i) {
    // PathIdentSegment ->
    // IDENTIFIER | "super" | "self" | "Self" | "crate"
    auto ident = node.segments()[i]->ident_segment()->ident();
    if(ident == "super") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"super\" is not supported here");
      return;
    } else if(ident == "self") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"self\" is not supported here");
      return;
    } else if(ident == "Self") {
      // set a temporary SelfType here.
      if(!_impl_type) {
        _recorder->tagged_report(kErrTypeInvalid, "Self type is invalid: might not be in impl block");
        return;
      }
      node.set_type(_impl_type);
      return;
    } else if(ident == "crate") {
      _recorder->tagged_report(kErrTypeNotResolved, "keyword \"crate\" is not supported here");
      return;
    }
    auto info = find_symbol(ident);
    if(!info) {
      _recorder->tagged_report(kErrTypeNotResolved, "TypePath identifier unrecognizable");
      return;
    }
    switch(info->kind) {
    case SymbolKind::kEnum: {
      // check whether the next one is a valid field.
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Enum type not determined");
        return;
      }
      if(i != node.segments().size() - 1) {
        _recorder->tagged_report(kErrTypeNotResolved, "Enum type as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kStruct: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Struct type not determined");
        return;
      }
      if(i != node.segments().size() - 1) {
        _recorder->tagged_report(kErrTypeNotResolved, "Struct type as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kPrimitiveType: {
      if(!info->type) {
        _recorder->tagged_report(kErrTypeNotResolved, "PrimitiveType type not determined");
        return;
      }
      if(i != node.segments().size() - 1) {
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
      if(i != node.segments().size() - 1) {
        _recorder->tagged_report(kErrTypeNotResolved, "TypeAlias as a type path interval");
        return;
      }
      node.set_type(info->type);
    } break;
    case SymbolKind::kTrait: {
      _recorder->tagged_report(kErrTypeInvalid, "TypePath for trait not implemented yet");
      return;
    } break;
    case SymbolKind::kFunction:
    case SymbolKind::kVariable:
    case SymbolKind::kConstant: {
      _recorder->tagged_report(kErrTypeInvalid, "Function/Constant/Variable Identifier as type path");
      return;
    } break;
    default: throw std::runtime_error("TypePath info kind invalid");
    }
  }
  if(!node.get_type()) {
    throw std::runtime_error("Fatal error: type not set in TypePath");
  }
}

void PreTypeFiller::postVisit(Function &node) {
  ScopedVisitor::postVisit(node); // exit the inner scope first. Add the symbol to the outer scope.
  std::vector<stype::TypePtr> params;
  stype::TypePtr caller_type;
  if(node.params_opt()) {
    if(auto &sp = node.params_opt()->self_param_opt()) {
      if(!is_in_asso_block()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Using self param outside impl items");
        return;
      }
      if(!_impl_type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Self type not resolved");
        return;
      }
      caller_type = _impl_type;
      if(sp->is_mut() && !sp->is_ref()) {
        _recorder->tagged_report(kErrTypeInvalid, "\"mut self\" is not supported");
        return;
      }
      if(sp->is_ref()) {
        caller_type = _type_pool->make_type<stype::RefType>(caller_type, sp->is_mut());
      }
    }
    for(auto &param: node.params_opt()->func_params()) {
      if(param->has_name()) {
        auto ptr = static_cast<FunctionParamPattern*>(param.get());
        auto tp = ptr->type()->get_type();
        if(!tp) {
          _recorder->tagged_report(kErrTypeNotResolved, "Function parameter pattern type tag not resolved");
          return;
        }
        params.push_back(tp);
      } else {
        auto ptr = static_cast<FunctionParamType*>(param.get());
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
  auto func_type = _type_pool->make_type<stype::FunctionType>(node.ident(), caller_type, std::move(params), ret_type);
  add_symbol(node.ident(), SymbolInfo{
    .node = &node, .ident = node.ident(), .kind = SymbolKind::kFunction, .type = func_type
  });
  if(is_in_asso_block()) {
    if(!_impl_type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Self type not resolved");
      return;
    }
    // register this function to the method table
    add_asso_method(_impl_type, func_type.get<stype::FunctionType>());
  }
}

void PreTypeFiller::postVisit(ConstantItem &node) {
  // ignore type tag...
  if(node.const_expr_opt()) {
    _evaluator.constEvaluate(*node.const_expr_opt(), _scopes);
    if(!node.const_expr_opt()->has_constant()) {
      _recorder->report("Const evaluation failed");
      return;
    }
    auto cval = node.const_expr_opt()->cval();
    auto info = find_symbol(node.ident());
    if(!info || info->kind != SymbolKind::kConstant) {
      _recorder->report("Invalid constant");
      return;
    }
    auto tp = node.type()->get_type();
    if(!tp) {
      _recorder->tagged_report(kErrTypeNotResolved, "Unresolved constant type tag");
      return;
    }
    if(!tp->is_convertible_from(*cval->type())) {
      _recorder->report("Const type tag " + tp->to_string()
        + " not convertible from value type " + cval->type()->to_string());
      return;
    }
    cval->set_type(tp);
    info->type = tp;
    info->cval = cval;
  }
}

void PreTypeFiller::postVisit(SelfParam &node) {
  auto tp = _impl_type;
  if(!tp) {
    _recorder->tagged_report(kErrTypeInvalid, "SelfParam not inside impl block");
    return;
  }
  if(node.is_mut() && !node.is_ref()) {
    _recorder->tagged_report(kErrTypeInvalid, "SelfParam does not support \"mut self\"");
    return;
  }
  if(node.type_opt()) {
    _recorder->tagged_report(kErrTypeInvalid, "SelfParam does not support type tag");
    return;
  }
  if(node.is_ref()) tp = _type_pool->make_type<stype::RefType>(tp, node.is_mut());
  node.set_type(tp);
}

void PreTypeFiller::visit(InherentImpl &node) {
  ScopedVisitor::preVisit(node); // enter impl scope

  node.type()->accept(*this);
  auto tp = node.type()->get_type();
  if(!tp) {
    _recorder->tagged_report(kErrTypeNotResolved, "InherentImpl target type not resolved");
    return;
  }
  _impl_type = tp;

  for(const auto &asso_item: node.asso_items())
    asso_item->accept(*this);

  ScopedVisitor::postVisit(node); // exit impl scope
}


void PreTypeFiller::postVisit(StructStruct &node) {
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

void PreTypeFiller::postVisit(Enumeration &node) {
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

void PreTypeFiller::postVisit(Crate &node) {
  auto info = find_symbol("main");
  if(!info || info->kind != SymbolKind::kFunction) {
    _recorder->report("Main function does not exist");
    ScopedVisitor::postVisit(node); // exit scope
    return;
  }
  auto func = info->type.get<stype::FunctionType>();
  if(func->self_type_opt() || !func->params().empty()) {
    _recorder->report("Main function shall not have parameters");
    ScopedVisitor::postVisit(node);
    return;
  }
  ScopedVisitor::postVisit(node); // exit scope
}


/********************** TypeFiller *****************************/

const std::string TypeFiller::kErrTypeNotResolved = "Error: Type not resolved";
const std::string TypeFiller::kErrTypeNotMatch = "Error: Type not match";
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

  // update: already registered in PreTypeFiller
  /*
  // after checking, register this function type to the symbol table.
  auto func_symbol = find_symbol(node.ident());
  if(!func_symbol) {
    throw std::runtime_error("Fatal error: invalid function name not checked");
  }

  std::vector<stype::TypePtr> params;
  if(auto ps = node.params_opt().get()) {
    if(auto s = ps->self_param_opt().get()) {
      // self param
      if(s->type_opt()) {
        _recorder->tagged_report(kErrTypeNotMatch, "No type tags for self param");
        return;
      }
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
  */
}

void TypeFiller::postVisit(StructStruct &node) {
  // no need.
}

void TypeFiller::postVisit(Enumeration &node) {
  // no need.
}

void TypeFiller::postVisit(EnumItem &node) {
  // no need... for now.
  node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32));
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
  // done in PreTypeFiller.
}

void TypeFiller::postVisit(TupleType &node) {
  // done in PreTypeFiller.
}

void TypeFiller::postVisit(ReferenceType &node) {
  // done in PreTypeFiller.
}

void TypeFiller::postVisit(ArrayType &node) {
  // done in PreTypeFiller
}

void TypeFiller::postVisit(SliceType &node) {
  // done in PreTypeFiller.
}

void TypeFiller::postVisit(TypePath &node) {
  // done in PreTypeFiller.
  // not needed.
}

void TypeFiller::postVisit(LiteralExpression &node) {
  auto tp = _type_pool->make_type<stype::PrimeType>(node.prime());
  if(node.prime() == stype::TypePrime::kStr)
    tp = _type_pool->make_type<stype::RefType>(tp, false);
  node.set_type(tp);
}

void TypeFiller::postVisit(PathInExpression &node) {
  if(node.segments().size() > 2) {
    // How did you get here with such a simplified Rust ruleset?
    throw std::runtime_error("PathInExpression with 2+ path separators not implemented yet");
    return;
  }
  if(node.segments().size() == 2) {
    // Enum variables or static asso items.
    // I can't do more.
    auto ident1 = node.segments().front()->ident_seg()->ident();
    auto ident2 = node.segments().back()->ident_seg()->ident();
    if(ident1 == "self" || ident1 == "crate" || ident1 == "super") {
      _recorder->tagged_report(kErrIdentNotResolved, "Not expected keywords " + std::string(ident1)
        + " before the end of PathInExpression");
      return;
    }
    auto info1 = find_symbol(ident1);
    // Struct::AssoItem.
    if((info1 && info1->kind == SymbolKind::kStruct) || ident1 == "Self") {
      stype::TypePtr stp;
      if(ident1 == "Self") {
        if(!is_in_asso_block()) {
          _recorder->tagged_report(kErrTypeNotResolved, "Invalid use of \"self\" outside an asso block");
          return;
        }
        if(!_impl_type) {
          _recorder->tagged_report(kErrTypeNotResolved, "Already ailed to resolve \"self\" here");
          return;
        }
        stp = _impl_type;
      } else {
        stp = info1->type;
      }
      // only support methods
      auto func = find_asso_method(stp, ident2, _type_pool);
      if(!func) {
        _recorder->tagged_report(kErrIdentNotResolved, "Not a recognizable method of " + stp->to_string());
        return;
      }
      node.set_type(stype::TypePtr(func));
      return;
    } else if(info1 && info1->kind == SymbolKind::kEnum) {
      auto enum_ptr = info1->type.get<stype::EnumType>();
      if(enum_ptr->vlist().contains(ident2)) {
        // found. type is still the enum type
        node.set_type(info1->type);
        return;
      } else {
        _recorder->tagged_report(kErrIdentNotResolved, "Enum variant name " + std::string(ident2)
          + " not found in enum type " + enum_ptr->to_string());
        return;
      }
    } else {
      _recorder->tagged_report(kErrIdentNotResolved, "Unrecognized PathInExpression");
      return;
    }
  } else {
    auto ident = node.segments().back()->ident_seg()->ident();
    if(ident == "self") {
      if(!is_in_asso_block()) {
        _recorder->tagged_report(kErrTypeNotResolved, "Invalid use of \"self\" outside an asso block");
        return;
      }
      if(!_impl_type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Already ailed to resolve \"self\" here");
        return;
      }
      stype::TypePtr tp;
      switch(_self_state) {
      case SelfState::kInvalid: {
        _recorder->tagged_report(kErrIdentNotResolved, "No self param is used here");
        return;
      } break;
      case SelfState::kNormal: {
        tp = _impl_type;
      } break;
      case SelfState::kRef: {
        tp = _type_pool->make_type<stype::RefType>(_impl_type, false);
      } break;
      case SelfState::kRefMut: {
        tp = _type_pool->make_type<stype::RefType>(_impl_type, true);
      } break;
      default: throw std::runtime_error("Unrecognizable SelfState");
      }
      node.set_type(tp);
      return;
    }
    if(ident == "super" || ident == "Self" || ident == "crate") {
      _recorder->tagged_report(kErrIdentNotResolved, "Not expected keywords " + std::string(ident)
        + " in the end of PathInExpression");
      return;
    }
    auto info = find_symbol(ident);
    if(!info) {
      _recorder->tagged_report(kErrIdentNotResolved, "Identifier not found as the end of PathInExpression");
      return;
    }
    if(info->kind == SymbolKind::kConstant || info->kind == SymbolKind::kVariable || info->kind == SymbolKind::kFunction
      || info->kind == SymbolKind::kStruct) {
      if(info->kind == SymbolKind::kVariable && info->is_place_mut) node.set_place_mut();
      node.set_type(info->type);
      } else {
        _recorder->tagged_report(kErrIdentNotResolved, "Invalid type associated with the identifier");
        return;
      }
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
  if(auto r = type.get_if<stype::RefType>()) {
    if(r->ref_is_mut()) node.set_place_mut();
    type = stype::TypePtr(r->inner());
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

  // requires to be a primitive type
  auto prime = type.get_if<stype::PrimeType>();
  if(!prime) {
    _recorder->tagged_report(kErrTypeNotMatch, "Negation on not primitive type");
    return;
  }

  switch(node.oper()) {
  case Operator::kSub: {
    if(!prime->is_number()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Negation on non number");
      return;
    }
    if(prime->is_unsigned_int()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Negation on unsigned integer");
      return;
    }
  } break;
  case Operator::kLogicalNot: {
    if(prime->prime() != stype::TypePrime::kBool && !prime->is_integer() && prime->prime() != stype::TypePrime::kInt) {
      _recorder->tagged_report(kErrTypeNotMatch, "Logical not on boolean or integer");
      return;
    }
  } break;
  default: throw std::runtime_error("Invalid negation operand");
  }
  node.set_type(type);
}

std::shared_ptr<stype::PrimeType> combine_prime(
  const std::shared_ptr<stype::PrimeType> &p1, const std::shared_ptr<stype::PrimeType> &p2) {
  if(p1 == p2) return p1;
  if(p1->prime() == stype::TypePrime::kInt)
    return p2->is_integer() ? p2 : nullptr;
  if(p2->prime() == stype::TypePrime::kInt)
    return p1->is_integer() ? p1 : nullptr;
  if(p1->prime() == stype::TypePrime::kFloat)
    return p2->is_float() ? p2 : nullptr;
  if(p2->prime() == stype::TypePrime::kFloat)
    return p1->is_float() ? p1 : nullptr;
  return nullptr;
}

void TypeFiller::postVisit(ArithmeticOrLogicalExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in ArithmeticOrLogicalExpression");
    return;
  }
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in ArithmeticOrLogicalExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in ArithmeticOrLogicalExpression."
      " type 1: " + type1->to_string() + " , type 2: " + type2->to_string());
    return;
  }
  switch(node.oper()) {
  case Operator::kShl:
  case Operator::kShr:
    if(prime1->prime() == stype::TypePrime::kInt) {
      // default
      type1 = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      prime1 = type1.get<stype::PrimeType>();
    }
    if(prime2->prime() == stype::TypePrime::kInt) {
      // default
      type2 = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      prime2 = type1.get<stype::PrimeType>();
    }
    if(prime1->is_integer() && prime2->is_integer()) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator << >>."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
    break;

  case Operator::kAdd:
  case Operator::kSub:
  case Operator::kMul:
  case Operator::kDiv:
  case Operator::kMod:
    if(auto p = combine_prime(prime1, prime2)) {
      node.set_type(stype::TypePtr(p));
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator + - * / %."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
    break;

  case Operator::kBitwiseAnd:
  case Operator::kBitwiseOr:
  case Operator::kBitwiseXor:
    if(auto p = combine_prime(prime1, prime2); p && prime1->is_integer()) {
      node.set_type(stype::TypePtr(p));
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator & | ^."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
    break;

  case Operator::kLogicalAnd:
  case Operator::kLogicalOr:
    if(prime1->prime() == stype::TypePrime::kBool && prime2->prime() == stype::TypePrime::kBool) {
      node.set_type(type2);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator && ||."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
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
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in ComparisonExpression");
    return;
  }
  switch(node.oper()) {
  case Operator::kGt:
  case Operator::kLt:
  case Operator::kGe:
  case Operator::kLe: {
    auto prime1 = type1.get_if<stype::PrimeType>();
    auto prime2 = type2.get_if<stype::PrimeType>();
    if(!prime1 || !prime2) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in ComparisonExpression");
      return;
    }
    if(combine_prime(prime1, prime2)) {
      node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool));
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator < > <= >=."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
  } break;
  case Operator::kEq:
  case Operator::kNe: {
    if(type1->is_convertible_from(*type2) || type2->is_convertible_from(*type1)) {
      node.set_type(_type_pool->make_type<stype::PrimeType>(stype::TypePrime::kBool));
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator == !=."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
  } break;
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
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in LazyBooleanExpression");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in LazyBooleanExpression");
    return;
  }
  switch(node.oper()) {
  case Operator::kLogicalAnd:
  case Operator::kLogicalOr:
    if(prime1->prime() == stype::TypePrime::kBool && prime2->prime() == stype::TypePrime::kBool) {
      node.set_type(type1);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator && ||."
        " type 1: " + type1->to_string() + ", type 2: " + type2->to_string());
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in TypeCastExpression");
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
    if(from_prime->is_undetermined() || from_prime->is_integer() || from_prime->is_float() ||
      from_prime->prime() == TypePrime::kChar || from_prime->prime() == TypePrime::kBool) {
      node.set_type(to_type);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Cannot cast type " + from_prime->to_string()
        + " to integer/floating point type " + to_prime->to_string());
      return;
    }
    break;
  default:
    if(from_prime->prime() == to_prime->prime()) {
      node.set_type(to_type);
    } else {
      _recorder->tagged_report(kErrTypeNotMatch, "Cannot perform type cast from " + from_prime->to_string()
        + " to " + to_prime->to_string());
      return;
    }
  }
}

void TypeFiller::visit(Statements &node) {
  RecursiveVisitor::preVisit(node);
  for(const auto &stmt: node.stmts()) {
    if(_is_in_main && _might_have_exited) {
      _recorder->report("Warning: Possible statements after exit()");
      return;
    }
    stmt->accept(*this);
  }
  if(node.expr_opt()) {
    if(_is_in_main && _might_have_exited) {
      _recorder->report("Warning: Possible statements after exit()");
      return;
    }
    node.expr_opt()->accept(*this);
  }
  RecursiveVisitor::postVisit(node);
}


void TypeFiller::preVisit(AssignmentExpression &node) {
  node.expr1()->set_lside();
}

void TypeFiller::postVisit(AssignmentExpression &node) {
  const auto to_type = node.expr1()->get_type();
  if(!to_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "To type not got in AssignmentExpression");
    return;
  }
  if(!node.expr1()->is_place_mut()) {
    _recorder->tagged_report(kErrNoPlaceMutability, "To type not place mutable. To type: "
      + to_type->to_string());
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
  if(node.expr1()->is_place_mut() && t->is_convertible_from(*f)) to_accept = true;
  auto rt = t.get_if<stype::RefType>(), rf = f.get_if<stype::RefType>();
  // &T <- &T...
  if(rt && rf && rt->inner()->is_convertible_from(*rf->inner()) && (!rt->ref_is_mut() || rf->ref_is_mut()))
    to_accept = true;
  // auto deref
  if(node.expr1()->allow_auto_deref() && rt && !rf && rt->ref_is_mut() && rt->inner()->is_convertible_from(*f))
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
    // self param: decide _self_state
    if(auto &sp = node.params_opt()->self_param_opt()) {
      if(sp->type_opt()) { throw std::runtime_error("Unchecked self param type tag"); }
      if(sp->is_mut()) {
        if(sp->is_ref()) {
          _self_state = SelfState::kRefMut;
        }
        else {
          _recorder->tagged_report(kErrTypeNotResolved, "\"mut self\" is not supported");
          return;
        }
      } else {
        if(sp->is_ref()) {
          _self_state = SelfState::kRef;
        } else {
          _self_state = SelfState::kNormal;
        }
      }
    }
    // other params: register symbol
    for(auto &param: node.params_opt()->func_params()) {
      if(param->has_name()) {
        auto ptr = static_cast<FunctionParamPattern*>(param.get());
        bind_pattern(ptr->pattern().get(), ptr->type()->get_type(), false); // to be considered...
      }
    }
  }

  if(node.ident() == "main" && !node.params_opt()) { _is_in_main = true; _might_have_exited = false; }

  if(node.body_opt()) node.body_opt()->accept(*this);

  if(node.ident() == "main" && !node.params_opt()) { _is_in_main = false; _might_have_exited = false; }

  // reset self state
  _self_state = SelfState::kInvalid;

  // check
  if(node.body_opt()) do {
    auto tp = node.body_opt()->get_type();
    if(!tp) {
      _recorder->tagged_report(kErrTypeNotResolved, "Function return type not resolved");
      break;
    }
    auto info = find_symbol(node.ident());
    auto f = info->type.get_if<stype::FunctionType>(); // ignore empty check...
    if(!f->ret_type()->is_convertible_from(*tp)
      || (tp.get_if<stype::NeverType>() && !f->ret_type().get_if<stype::NeverType>())) {
      _recorder->tagged_report(kErrTypeNotMatch, "Function return type not match with its body."
        " tag: " + f->ret_type()->to_string() + ", evaluation: " + tp->to_string());
      break;
    }
  } while(false);

  ScopedVisitor::postVisit(node);
}


void TypeFiller::postVisit(CompoundAssignmentExpression &node) {
  auto type1 = node.expr1()->get_type();
  if(!type1) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 1 not got in CompoundAssignmentExpression");
    return;
  }
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in CompoundAssignmentExpression");
    return;
  }
  // Accepted Compound Assignment: (rt <- rf)
  // mut P <- P
  // (auto deref acceptable) &mut P <- P
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1) {
    auto r = type1.get_if<stype::RefType>();
    if(!r->ref_is_mut()) {
      _recorder->tagged_report(kErrTypeNotResolved, "Type not supported in CompoundAssignmentExpression."
        " type 1:" + type1->to_string() + ", type 2: " + type2->to_string());
      return;
    }
    prime1 = r->inner().get_if<stype::PrimeType>();
  }
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not supported in CompoundAssignmentExpression."
      " type 1:" + type1->to_string() + ", type 2: " + type2->to_string());
    return;
  }
  switch(node.oper()) {
  case Operator::kShlAssign:
  case Operator::kShrAssign:
    if(prime1->prime() == stype::TypePrime::kInt) {
      // default
      type1 = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      prime1 = type1.get<stype::PrimeType>();
    }
    if(prime2->prime() == stype::TypePrime::kInt) {
      // default
      type2 = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      prime2 = type1.get<stype::PrimeType>();
    }
    if(!(prime1->is_integer() && prime2->is_integer())) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator <<= >>=. type 1: " + type1->to_string()
        + ", type 2: " + type2->to_string());
      return;
    }
    break;

  case Operator::kAddAssign:
  case Operator::kSubAssign:
  case Operator::kMulAssign:
  case Operator::kDivAssign:
  case Operator::kModAssign:
    if(combine_prime(prime1, prime2) != prime1) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator += -= *= /= %=");
      return;
    }
    break;

  case Operator::kBitwiseAndAssign:
  case Operator::kBitwiseOrAssign:
  case Operator::kBitwiseXorAssign:
    if(!(combine_prime(prime1, prime2) == prime1 && prime1->is_integer())) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type invalid in operator &= |= ^=");
      return;
    }
    break;

  default:
    throw std::runtime_error("Invalid operator type in compound assignment expression");
  }
  node.set_type(type1);
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
      auto t2 = elem->get_type();
      if(!t2) {
        _recorder->tagged_report(kErrTypeNotResolved, "Type not got in ArrayExpression");
        return;
      }
      if(!type->is_convertible_from(*t2) && !t2->is_convertible_from(*type)) {
        _recorder->tagged_report(kErrTypeNotMatch, "Array type not same: "
          + type->to_string() + " vs " + t2->to_string());
        return;
      }
      if(t2->is_convertible_from(*type)) type = t2;
    }
    node.set_type(_type_pool->make_type<stype::ArrayType>(type, arr->expr_list().size()));
  } else {
    auto arr = dynamic_cast<RepeatedArrayElements*>(node.elements_opt().get());
    if(!arr) {
      throw std::runtime_error("Repeated array collapsed");
      return;
    }
    _evaluator.constEvaluate(*arr->len_expr(), _scopes);
    if(!arr->len_expr()->has_constant()) {
      _recorder->tagged_report(kErrConstevalFailed, "Type length not evaluated");
      return;
    }
    auto length_prime = arr->len_expr()->cval()->get_if<sconst::ConstPrime>();
    if(!length_prime) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not a primitive");
      return;
    }
    auto length_opt = length_prime->get_usize();
    if(!length_opt) {
      _recorder->tagged_report(kErrTypeNotMatch, "Type length not an unsigned value");
      return;
    }
    auto type = arr->val_expr()->get_type();
    if(!type) {
      _recorder->tagged_report(kErrTypeNotResolved, "Expr type not got in ArrayExpression");
      return;
    }
    node.set_type(_type_pool->make_type<stype::ArrayType>(type, *length_opt));
  }
}

void TypeFiller::preVisit(IndexExpression &node) {
  if(node.is_lside()) {
    node.expr_obj()->set_lside();
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
  // In fact, we need an is_unsigned() here...
  // But the testcases say we shall allow i32. Make them happy.
  if(!index_primitive || !(index_primitive->is_integer() || index_primitive->prime() == stype::TypePrime::kInt)) {
    _recorder->tagged_report(kErrTypeNotMatch, "Index not an integer");
    return;
  }

  // (mut1) [T; N] -> (mut1) &(mut1) T (lside), T (rside)
  // (mut1) &(mut2) [T; N] -> (mut2) &(mut2) T (lside), T (rside)

  if(auto a = expr_type.get_if<stype::ArrayType>()) {
    auto tp = a->inner();
    if(node.is_lside()) {
      bool is_mut1 = node.expr_obj()->is_place_mut();
      if(is_mut1) node.set_place_mut();
      tp = _type_pool->make_type<stype::RefType>(tp, is_mut1);
    }
    node.set_type(tp);
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
    bool is_mut2 = r->ref_is_mut();
    if(node.is_lside()) {
      if(is_mut2) node.set_place_mut();
      tp = _type_pool->make_type<stype::RefType>(tp, is_mut2);
    }
    node.set_type(tp);
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
  // check given type
  auto struct_tp = node.path()->get_type();
  if(!struct_tp) {
    _recorder->tagged_report(kErrTypeNotResolved, "Struct constructor name not a recognized struct");
    return;
  }
  auto stp = struct_tp.get_if<stype::StructType>();
  if(!stp) {
    _recorder->tagged_report(kErrTypeNotResolved, "Struct constructor name not a recognized struct");
    return;
  }
  std::unordered_map<StringRef, stype::TypePtr> counter;
  for(auto &[ident, t]: stp->fields()) {
    counter.emplace(ident, t);
  }
  if(node.fields_opt()) for(auto &field: node.fields_opt()->fields()) {
    if(field->is_named()) {
      auto ptr = static_cast<NamedStructExprField*>(field.get());
      auto expr_tp = ptr->expr()->get_type();
      if(!expr_tp) {
        _recorder->tagged_report(kErrTypeNotResolved, "Field expr type not resolved");
        return;
      }
      auto it = counter.find(ptr->ident());
      if(it == counter.end()) {
        _recorder->tagged_report(kErrIdentNotResolved, "Not a recognized field identifier");
        return;
      }
      if(!it->second) {
        _recorder->tagged_report(kErrTypeNotResolved, "Duplicated field identifier");
        return;
      }
      if(!it->second->is_convertible_from(*expr_tp)) {
        _recorder->tagged_report(kErrTypeNotMatch, "Field type not match");
        return;
      }
      it->second = stype::TypePtr();
    } else {
      auto ptr = static_cast<IndexStructExprField*>(field.get());
      _recorder->report("IndexStructExprField not supported");
      return;
    }
  }
  for(auto &[ident, t]: counter) {
    if(t) {
      _recorder->report("StructExpression did not initialize all fields");
      return;
    }
  }
  node.set_type(struct_tp);
}

void TypeFiller::postVisit(CallExpression &node) {
  auto func_type = node.expr()->get_type().get_if<stype::FunctionType>();
  if(!func_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "Function call type not resolved");
    return;
  }
  if(func_type->ident() == "exit") {
    if (!_is_in_main) {
      // builtin check: exit function must be in main function.
      _recorder->report("Warning: exit() function called out of main function");
      return;
    }
    _might_have_exited = true;
  }
  if(!node.params_opt()) {
    // no params.
    if(!func_type->params().empty()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Function has 0 parameters,"
        " but is called with" + std::to_string(node.params_opt()->expr_list().size()) + " arguments");
      return;
    }
  } else {
    const auto &argv = node.params_opt()->expr_list();
    const auto &params = func_type->params();
    if(argv.size() != params.size()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Function has " + std::to_string(params.size()) + " parameters,"
        " but is called with" + std::to_string(argv.size()) + " arguments");
      return;
    }
    for(int i = 0; i < argv.size(); ++i) {
      auto expr_type = argv[i]->get_type();
      if(!expr_type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Function parameter type not resolved");
        return;
      }
      if(!params[i]->is_convertible_from(*expr_type)) {
        _recorder->tagged_report(kErrTypeNotMatch, "Function type \"" + func_type->to_string() + "\" expected "
          + std::to_string(i) + "-th param to be " + params[i]->to_string() + ", but received "
          + expr_type->to_string());
        return;
      }
      if(auto p = expr_type.get_if<stype::PrimeType>(); p && p->is_undetermined()) {
        auto succeed = argv[i]->set_type(params[i]);
        if(!succeed) {
          _recorder->tagged_report(kErrConstevalFailed, "Function param conversion faces problems like overflow");
          return;
        }
      }
    }
  }
  node.set_type(func_type->ret_type());
}

void TypeFiller::preVisit(MethodCallExpression &node) {
  node.expr()->set_lside();
}

void TypeFiller::postVisit(MethodCallExpression &node) {
  auto caller = node.expr()->get_type();
  if(!caller) {
    _recorder->tagged_report(kErrTypeNotResolved, "Method caller type not resolved");
    return;
  }
  // if kInt/kFloat: use default type.
  // I forgot the p != nullptr, but it didn't break out when I made a flaw in FuncType builtin... weird.
  if(auto p = caller.get_if<stype::PrimeType>(); p && p->is_undetermined()) {
    if(p->prime() == stype::TypePrime::kInt) {
      caller = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      node.expr()->set_type(caller);
    }
    if(p->prime() == stype::TypePrime::kFloat) {
      caller = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32);
      node.expr()->set_type(caller);
    }
  }
  // allow auto deref
  // Assume caller = T and find methods of T.
  auto func_ptr = find_asso_method(caller, node.segment()->ident_seg()->ident(), _type_pool);
  if(!func_ptr) {
    // Assume caller = &T / &mut T and find methods of T.
    if(auto r = caller.get_if<stype::RefType>()) {
      func_ptr = find_asso_method(r->inner(), node.segment()->ident_seg()->ident(), _type_pool);
    } else {
      _recorder->tagged_report(kErrIdentNotResolved, "Method not found");
      return;
    }
    if(!func_ptr) {
      _recorder->tagged_report(kErrIdentNotResolved, "Method not found");
      return;
    }
  }
  // check whether the arguments passed fit the parameters.
  // allow auto ref
  // caller = T, caller_req = T / &T
  // caller = mut T, caller_req = T / &T / &mut T
  // caller = &T, caller_req = &T
  // caller = &mut T, caller_req = &T, &mut T
  int pos = 0;
  if(auto caller_req = func_ptr->self_type_opt()) {
    bool is_valid = false;
    if(caller_req->is_convertible_from(*caller)) {
      // caller = T / mut T, caller_req = T
      is_valid = true;
    } else {
      auto tc = caller.get_if<stype::RefType>();
      auto tcr = caller_req.get_if<stype::RefType>();
      if(tcr && tcr->inner()->is_convertible_from(*caller) && (node.expr()->is_place_mut() || !tcr->ref_is_mut())) {
        // caller = T, caller_req = &T
        // caller = mut T, caller_req = &T / &mut T
        is_valid = true;
      }
      // inner type shall be all determined... Right?
      if(tc && tcr && (tc->ref_is_mut() || !tcr->ref_is_mut()) && (tcr->inner() == tc->inner())) {
        // caller = &T, caller_req = &T
        // caller = &mut T, caller_req = &T, &mut T
        is_valid = true;
      }
    }
    if(!is_valid) {
      _recorder->tagged_report(kErrTypeNotMatch, "Invalid method caller: cannot match required self type."
        " Caller type: " + caller->to_string() + ", required self type: " + caller_req->to_string());
      return;
    }
    pos = 1;
  }
  if(!node.params_opt()) {
    if(!func_ptr->params().empty()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Method call param number mismatch. Method needs "
        + std::to_string(func_ptr->params().size()) + "params, but given 0 args");
      return;
    }
    // else 0-0 is valid.
  } else {
    auto &argv = node.params_opt()->expr_list();
    auto &params = func_ptr->params();
    if(argv.size() != params.size()) {
      _recorder->tagged_report(kErrTypeNotMatch, "Method call param number mismatch. Method needs "
        + std::to_string(params.size()) + "params, but given " + std::to_string(argv.size()) + " args");
      return;
    }
    for(int i = 0; i < argv.size(); ++i) {
      auto expr_type = argv[i]->get_type();
      if(!expr_type) {
        _recorder->tagged_report(kErrTypeNotResolved, "Function parameter type not resolved");
        return;
      }
      if(!params[i]->is_convertible_from(*expr_type)) {
        _recorder->tagged_report(kErrTypeNotMatch, "Function type \"" + func_ptr->to_string() + "\" expected "
          + std::to_string(i) + "-th param to be " + params[i]->to_string() + ", but received "
          + expr_type->to_string());
        return;
      }
      if(auto p = expr_type.get_if<stype::PrimeType>(); p && p->is_undetermined()) {
        auto succeed = argv[i]->set_type(params[i]);
        if(!succeed) {
          _recorder->tagged_report(kErrConstevalFailed, "Function param conversion faces problems like overflow");
          return;
        }
      }
    }
  }
  // happy now.
  node.set_type(func_ptr->ret_type());
}

void TypeFiller::preVisit(FieldExpression &node) {
  if(node.is_lside()) {
    node.expr()->set_lside();
  }
}

void TypeFiller::postVisit(FieldExpression &node) {
  // auto deref may happen here.
  auto type = node.expr()->get_type();
  if(!type) {
    _recorder->tagged_report(kErrTypeNotResolved, "FieldExpression type not resolved");
    return;
  }
  // Assume struct type T has a field x: X, consider a.x with different type of a.
  // allowed circumstances:
  // a: T, a.x: X (must rside)
  // a: mut T, a.x: mut X (lside), X (rside)
  // a: (mut) &T, a.x: X (must rside)
  // a: (mut) &mut T, a.x: &mut X (lside), X (rside)
  if(auto s = type.get_if<stype::StructType>()) {
    auto it = s->fields().find(node.ident());
    if(it == s->fields().end()) {
      _recorder->tagged_report(kErrIdentNotResolved, "Field ident not found in type " + s->to_string());
      return;
    }
    // a: T, a.x: X (must rside)
    // a: mut T, a.x: mut X (lside), X (rside)
    node.set_type(it->second);
    if(node.expr()->is_place_mut()) {
      node.set_place_mut();
    } else if(node.is_lside()) {
      _recorder->tagged_report(kErrNoPlaceMutability, "Field mutability violated");
      return;
    }
    return;
  }
  if(auto r = type.get_if<stype::RefType>()) {
    if(auto s = r->inner().get_if<stype::StructType>()) {
      auto it = s->fields().find(node.ident());
      if(it == s->fields().end()) {
        std::string msg;
        for(auto &[k, v]: s->fields()) msg = msg + k + " ";
        _recorder->tagged_report(kErrIdentNotResolved, "Field ident not found in type " + s->to_string()
          + ". Target: " + std::string(node.ident()) + ", candidates: " + msg);
        return;
      }
      // a: (mut) &T, a.x: X (must rside)
      // a: (mut) &mut T, a.x: mut &mut X (lside), X (rside)
      if(r->ref_is_mut()) {
        if(node.is_lside()) {
          node.set_place_mut();
          node.set_type(_type_pool->make_type<stype::RefType>(it->second, true));
        } else {
          node.set_type(it->second);
        }
      } else {
        if(node.is_lside()) {
          _recorder->tagged_report(kErrNoPlaceMutability, "Field mutability violated");
          return;
        }
        node.set_type(it->second);
      }
      return;
    }
  }
  _recorder->tagged_report(kErrTypeNotMatch, "Unsupported field operation in type " + type->to_string()
    + ": can't be seen as a struct");
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
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in RangeExpr");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in RangeExpr");
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in RangeFromExpr");
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in RangeToExpr");
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
  auto type2 = node.expr2()->get_type();
  if(!type2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type 2 not got in RangeInclusiveExpr");
    return;
  }
  auto prime1 = type1.get_if<stype::PrimeType>();
  auto prime2 = type2.get_if<stype::PrimeType>();
  if(!prime1 || !prime2) {
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in RangeInclusiveExpr");
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
    _recorder->tagged_report(kErrTypeNotResolved, "Type not primitive in RangeToInclusiveExpr");
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
    ScopedVisitor::postVisit(node); // exit this scope
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
      ScopedVisitor::postVisit(node); // exit this scope
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
  ScopedVisitor::postVisit(node); // exit this scope
}

void TypeFiller::postVisit(FunctionBodyExpr &node) {
  stype::TypePtr rt; // the type that "return"s imply
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

      if(!t->is_convertible_from(*t2) && !t2->is_convertible_from(*t)) {
        _recorder->tagged_report(kErrTypeNotMatch, "FunctionBodyExpression has different return type,"
          " " + t->to_string() + " vs " + t2->to_string());
        return;
      }
      if(t2->is_convertible_from(*t)) t = t2;
    }
    rt = t;
  }

  stype::TypePtr st; // the type that tail expression implies. Might be NeverType due to control flow termination.
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
    if(!rt->is_coercible_from(*st) && !st->is_coercible_from(*rt)) {
      _recorder->tagged_report(kErrTypeNotMatch, "Func eval type not the same with return type tag: "
        "eval type: "+ st->to_string() + ", return type tag: " + rt->to_string());
      return;
    }
    if(rt->is_coercible_from(*st)) st = rt;
    node.set_type(st);
  }
}


void TypeFiller::postVisit(InfiniteLoopExpression &node) {
  // loop {...}
  if(node.loop_breaks().empty()) {
    node.set_type(_type_pool->make_type<stype::NeverType>());
    return;
  }
  auto type =
    node.loop_breaks().front()->expr_opt()
    ? node.loop_breaks().front()->expr_opt()->get_type()
    : _type_pool->make_unit();
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
    if(!t->is_coercible_from(*type) && !type->is_coercible_from(*t)) {
      _recorder->tagged_report(kErrTypeNotMatch, "InfiniteLoopExpression has different return type. "
        + t->to_string() + " vs " + type->to_string());
      return;
    }
    if(t->is_coercible_from(*type)) type = t;
  }
  if(node.block_expr()->always_returns())
    node.set_always_returns();
  node.set_type(type);
}

void TypeFiller::postVisit(PredicateLoopExpression &node) {
  // while ... {}
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "While condition type not resolved");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimeType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "While condition must be a boolean, not " + cond_type->to_string());
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
    if(!t->is_coercible_from(*type) && !type->is_coercible_from(*t)) {
      _recorder->tagged_report(kErrTypeNotMatch, "PredicateLoopExpression has different return type. "
        + t->to_string() + " vs " + type->to_string());
      return;
    }
    if(t->is_coercible_from(*type)) type = t;
  }

  if(node.cond()->expr()->always_returns() || node.block_expr()->always_returns())
    node.set_always_returns();
  node.set_type(type);
}

void TypeFiller::postVisit(IfExpression &node) {
  // if ... {} else ...
  auto cond_type = node.cond()->expr()->get_type();
  if(!cond_type) {
    _recorder->tagged_report(kErrTypeNotResolved, "If condition type not resolved");
    return;
  }
  auto cond_prime = cond_type.get_if<stype::PrimeType>();
  if(!cond_prime || cond_prime->prime() != stype::TypePrime::kBool) {
    _recorder->tagged_report(kErrTypeNotMatch, "If condition must be a boolean, not " + cond_type->to_string());
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
      stype::TypePtr t2 = arg->get_type();
      if(!t2) {
        success = false;
        return;
      }
      if(!type->is_coercible_from(*t2) && !t2->is_coercible_from(*type)) {
        success = false;
      }
      if(t2->is_coercible_from(*type)) type = t2;
      if(!arg->always_returns()) always_returns = false;
    } else {
      stype::TypePtr t2 = _type_pool->make_unit();
      if(!type->is_coercible_from(*t2) && !t2->is_coercible_from(*type)) {
        success = false;
      }
      if(t2->is_coercible_from(*type)) type = t2;
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
    // (auto deref) allow T <- &(mut) T
    if(!type_tag) {
      _recorder->tagged_report(kErrIdentNotResolved, "Type tag not resolved");
      return;
    }
    // heh. return13: allow never type.
    bool allow_convert = type_tag->is_coercible_from(*type);
    if(auto r = type.get_if<stype::RefType>();
      node.expr_opt()->allow_auto_deref() && r && type_tag->is_convertible_from(*r->inner())) {
      allow_convert = true;
    }
    if(!allow_convert) {
      _recorder->tagged_report(kErrTypeNotMatch, "type mismatch inside let statement."
        " Type tag: " + type_tag->to_string() + "; Expr type: " + type->to_string());
      return;
    }
    type = type_tag;
  }
  // with no tag, we need to determine a specific type (kInt -> kI32, kFloat -> kF32)
  bind_pattern(node.pattern().get(), type, !node.type_opt());
}

void TypeFiller::bind_pattern(PatternNoTopAlt *pattern, stype::TypePtr type, bool need_spec) {
  if(auto ident_p = dynamic_cast<IdentifierPattern*>(pattern)) {
    bind_identifier(ident_p, type, need_spec);
  } else if(auto wildcard_p = dynamic_cast<WildcardPattern*>(pattern)) {
    bind_wildcard(wildcard_p, type, need_spec);
  } else if(auto tuple_p = dynamic_cast<TuplePattern*>(pattern)) {
    bind_tuple(tuple_p, type, need_spec);
  } else if(auto struct_p = dynamic_cast<StructPattern*>(pattern)) {
    bind_struct(struct_p, type, need_spec);
  } else if(auto ref_p = dynamic_cast<ReferencePattern*>(pattern)) {
    bind_reference(ref_p, type, need_spec);
  } else if(auto literal_p = dynamic_cast<LiteralPattern*>(pattern)) {
    bind_literal(literal_p, type, need_spec);
  } else if(auto grouped_p = dynamic_cast<GroupedPattern*>(pattern)) {
    bind_grouped(grouped_p, type, need_spec);
  } else if(auto slice_p = dynamic_cast<SlicePattern*>(pattern)) {
    bind_slice(slice_p, type, need_spec);
  } else if(auto path_p = dynamic_cast<PathPattern*>(pattern)) {
    bind_path(path_p, type, need_spec);
  } else {
    _recorder->tagged_report(kErrTypeNotResolved, "Unsupported type in resolution");
  }
}

void TypeFiller::bind_identifier(IdentifierPattern *pattern, stype::TypePtr type, bool need_spec) {
  // always success

  if(need_spec) {
    if(auto p = type.get_if<stype::PrimeType>()) {
      if(p->prime() == stype::TypePrime::kInt) type = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kI32);
      if(p->prime() == stype::TypePrime::kFloat) type = _type_pool->make_type<stype::PrimeType>(stype::TypePrime::kF32);
    }
  }

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
  auto info_ptr = find_symbol(pattern->ident());
  SymbolInfo info{
    .node = pattern, .ident = pattern->ident(), .kind = SymbolKind::kVariable,
    .type = type, .is_place_mut = is_place_mut
  };
  if(info_ptr) {
    // variable symbol occurred.
    info_ptr = _scopes.back()->find_symbol(pattern->ident());
    if(info_ptr) {
      // shadow the same symbol in the same scope.
      *info_ptr = info;
    } else {
      // override the symbol in upper scope.
      add_symbol(pattern->ident(), info);
    }
  } else {
    // new symbol. add to current scope.
    add_symbol(pattern->ident(), info);
  }
}

void TypeFiller::bind_wildcard(WildcardPattern *pattern, stype::TypePtr type, bool need_spec) {
  // always success
  // nothing to do here
}

void TypeFiller::bind_tuple(TuplePattern *pattern, stype::TypePtr type, bool need_spec) {
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
    bind_pattern(sub_pattern->patterns()[i].get(), sub_type, need_spec);
  }
}

void TypeFiller::bind_struct(StructPattern *pattern, stype::TypePtr type, bool need_spec) {
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
      bind_pattern(sub_pattern->patterns().front().get(), it->second, need_spec);
    } else {
      _recorder->report("Type binding failed: field identifier not exist");
    }
  }
}

void TypeFiller::bind_reference(ReferencePattern *pattern, stype::TypePtr type, bool need_spec) {
  if(auto r = type.get_if<stype::RefType>(); !r) {
    _recorder->report("Type binding failed: not a reference");
    return;
  } else if(pattern->is_mut() ^ r->ref_is_mut()) {
    _recorder->report("Type binding failed: reference mutability mismatch");
    return;
  }
  auto sub_pattern = pattern->pattern().get();
  bind_pattern(sub_pattern, type, need_spec);
}

void TypeFiller::bind_literal(LiteralPattern *pattern, stype::TypePtr type, bool need_spec) {
  _recorder->report("Type binding error: literal pattern not supported in let statement");
}

void TypeFiller::bind_grouped(GroupedPattern *pattern, stype::TypePtr type, bool need_spec) {
  auto sub_pattern = pattern->pattern().get();
  if(sub_pattern->patterns().size() != 1) {
    _recorder->report("Type binding failed: multi pattern not supported yet");
    return;
  }
  bind_pattern(sub_pattern->patterns().front().get(), type, need_spec);
}

void TypeFiller::bind_slice(SlicePattern *pattern, stype::TypePtr type, bool need_spec) {
  _recorder->report("Slice Not supported yet");
}

void TypeFiller::bind_path(PathPattern *pattern, stype::TypePtr type, bool need_spec) {
  _recorder->report("Type binding error: path pattern not supported in let statement");
}

}