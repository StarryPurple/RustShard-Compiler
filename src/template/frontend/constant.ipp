#ifndef RUST_SHARD_FRONTEND_CONSTANT_IPP
#define RUST_SHARD_FRONTEND_CONSTANT_IPP

#include "constant.h"

namespace insomnia::rust_shard::sconst {

template <class T> requires const_val_list::contains<T>
const T& ConstValue::get() const {
  using stype::TypeKind;
  using stype::TypePrime;
  using stype::PrimeType;

  return std::get<T>(_const_val);
  /*
  if(_type && _type->kind() == TypeKind::kPrimitive) {
    if constexpr(std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>) {
      if constexpr(std::is_signed_v<T>) {
        return static_cast<T>(std::get<std::int64_t>(std::get<ConstPrime>(_const_val).value));
      } else {
        return static_cast<T>(std::get<std::uint64_t>(std::get<ConstPrime>(_const_val).value));
      }
    } else {
      return std::get<T>(std::get<ConstPrime>(_const_val).value);
    }
  } else {
    return std::get<T>(_const_val);
  }
  */
}

template <class T> requires const_val_list::contains<T>
const T* ConstValue::get_if() const {
  using stype::TypeKind;
  using stype::TypePrime;
  using stype::PrimeType;
  return std::get_if<T>(&_const_val);
  /*
  if constexpr(type_utils::is_one_of<
    T,
    ConstReference,
    ConstRange,
    ConstTuple,
    ConstArray,
    ConstStruct,
    ConstSlice>) {
    return std::get_if<T>(&_const_val);
  } else if constexpr(std::is_same_v<T, ConstPrimitive>) {
    if(!_type || _type->kind() != TypeKind::kPrimitive) {
      throw std::runtime_error("Error in ConstValue: type pointer and storage not match");
    }
    const auto *primitive_val_ptr = std::get_if<ConstPrimitive>(&_const_val);
    if(!primitive_val_ptr) return nullptr;
    if constexpr(std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>) {
      auto prime = _type.get<PrimitiveType>()->prime();
      if constexpr(std::is_signed_v<T>) {
        if(prime >= TypePrime::kI8 && prime <= TypePrime::kI64 || prime == TypePrime::kISize) {
          return reinterpret_cast<const T*>(std::get_if<std::int64_t>(&primitive_val_ptr->value));
        }
      } else {
        if(prime >= TypePrime::kU8 && prime <= TypePrime::kU64 || prime == TypePrime::kUSize) {
          return reinterpret_cast<const T*>(std::get_if<std::uint64_t>(&primitive_val_ptr->value));
        }
      }
    } else {
      return std::get_if<T>(&primitive_val_ptr->value);
    }
  } else {
    // monostate
    return nullptr;
  }
  */
}

}


#endif // RUST_SHARD_FRONTEND_CONSTANT_IPP