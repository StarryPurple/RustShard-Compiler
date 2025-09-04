#include "ast_constant.h"

namespace insomnia::rust_shard::sem_const {
bool ConstValPtr::operator==(const ConstValPtr &other) const {
  if(!_ptr || !other._ptr) return _ptr == other._ptr;
  return *_ptr == *other._ptr;
}

template <class T>
const T& ConstValue::get() const {
  using sem_type::TypeKind;
  using sem_type::TypePrime;
  using sem_type::PrimitiveType;

  if(_type && _type->kind() == TypeKind::kPrimitive) {
    if constexpr(std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>) {
      if constexpr(std::is_signed_v<T>) {
        return static_cast<T>(std::get<std::int64_t>(std::get<ConstPrimitive>(_const_val).value));
      } else {
        return static_cast<T>(std::get<std::uint64_t>(std::get<ConstPrimitive>(_const_val).value));
      }
    } else {
      return std::get<T>(std::get<ConstPrimitive>(_const_val).value);
    }
  } else {
    return std::get<T>(_const_val);
  }
}

template <class T>
const T* ConstValue::get_if() const {
  using sem_type::TypeKind;
  using sem_type::TypePrime;
  using sem_type::PrimitiveType;

  if(_type && _type->kind() == TypeKind::kPrimitive) {
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
    return std::get_if<T>(&_const_val);
  }
  return nullptr;
}

template <class T>
void ConstValue::set(sem_type::TypePtr type, const T &value) {
  _type = std::move(type);
  using sem_type::TypeKind;
  using sem_type::PrimitiveType;

  if(_type && _type->kind() == TypeKind::kPrimitive) {
    if constexpr(std::is_integral_v<T> && !std::is_same_v<T, char> && !std::is_same_v<T, bool>) {
      if constexpr(std::is_signed_v<T>) {
        _const_val = ConstPrimitive(static_cast<std::int64_t>(value));
      } else {
        _const_val = ConstPrimitive(static_cast<std::uint64_t>(value));
      }
    } else {
      _const_val = ConstPrimitive(value);
    }
  } else if constexpr(std::is_base_of_v<ConstBase, T>) {
    _const_val = value;
  } else {
    throw std::invalid_argument("Unsupported type for ConstValue set");
  }
}
}
