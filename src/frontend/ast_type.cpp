#include "ast_type.h"

#include <unordered_map>

namespace insomnia::rust_shard::sem_type {

std::string_view get_type_view_from_prime(TypePrime prime) {
  static const std::unordered_map<TypePrime, std::string_view> table = {
    {TypePrime::kChar, "char"}, {TypePrime::kBool, "bool"},
    {TypePrime::kI8, "i8"}, {TypePrime::kI16, "i16"},
    {TypePrime::kI32, "i32"}, {TypePrime::kI64, "i64"},
    {TypePrime::kU8, "u8"}, {TypePrime::kU16, "u16"},
    {TypePrime::kU32, "u32"}, {TypePrime::kU64, "u64"},
    {TypePrime::kISize, "isize"}, {TypePrime::kUSize, "usize"},
    {TypePrime::kF32, "f32"}, {TypePrime::kF64, "f64"},
    {TypePrime::kString, "str"}
  };
  return table.at(prime);
}

const std::vector<TypePrime>& type_primes() {
  static const std::vector<TypePrime> table = {
    TypePrime::kChar, TypePrime::kBool,
    TypePrime::kI8, TypePrime::kI16,
    TypePrime::kI32, TypePrime::kI64,
    TypePrime::kU8, TypePrime::kU16,
    TypePrime::kU32, TypePrime::kU64,
    TypePrime::kISize, TypePrime::kUSize,
    TypePrime::kF32, TypePrime::kF64,
    TypePrime::kString
  };
  return table;
}

bool TypePtr::operator==(const TypePtr &other) const {
  if(!_ptr || !other._ptr) return _ptr == other._ptr;
  return *_ptr == *other._ptr;
}

std::size_t ExprType::hash() const {
  std::size_t seed = 0;
  this->combine_hash(seed);
  return seed;
}

bool ExprType::operator==(const ExprType &other) const  {
  if(_kind != other._kind) return false;
  return this->equals_impl(other);
}

void ExprType::combine_hash_impl(std::size_t &seed, std::size_t h) {
  static constexpr std::size_t HASH_MAGIC_NUM = 0x9e3779b9;
  seed ^= HASH_MAGIC_NUM + h + (seed << 6) + (seed >> 2);
}

void PrimitiveType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(_prime));
}

bool PrimitiveType::equals_impl(const ExprType &other) const {
  return _prime == static_cast<const PrimitiveType&>(other).prime();
}

void ArrayType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _type->combine_hash(seed);
  combine_hash_impl(seed, _length);
}

bool ArrayType::equals_impl(const ExprType &other) const {
  const auto &other_array = static_cast<const ArrayType&>(other);
  if(_length != other_array.length()) return false;
  return *_type == *other_array.type();
}

void ReferenceType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _type->combine_hash(seed);
}

bool ReferenceType::equals_impl(const ExprType &other) const {
  return *_type == *static_cast<const ReferenceType&>(other).type();
}

void StructType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<std::string_view> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
  // not rely on fields
  /*
  for(auto &[name, type]: _fields) {
    combine_hash_impl(seed, static_cast<std::size_t>(hasher(name)));
    type->combine_hash(seed);
  }
  */
}

bool StructType::equals_impl(const ExprType &other) const {
  const auto &other_struct = static_cast<const StructType&>(other);
  if(_ident != other_struct.ident()) return false;
  // not rely on fields
  /*
  const auto &other_fields = other_struct.fields();
  if(_fields.size() != other_fields.size()) return false;
  for(auto it = _fields.begin(), other_it = other_fields.begin();
    other_it != other_fields.end(); ++it, ++other_it) {
    if(it->first != other_it->first) return false;
    if(*it->second != *other_it->second) return false;
  }
  */
  return true;
}

void TupleType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  for(auto &type: _members) type->combine_hash(seed);
}

bool TupleType::equals_impl(const ExprType &other) const {
  const auto &other_tuple = static_cast<const TupleType&>(other);
  const auto &other_members = other_tuple.members();
  if(_members.size() != other_members.size()) return false;
  for(auto it = _members.begin(), other_it = other_members.begin();
    it != _members.end(); ++it, ++other_it) {
    if(**it != **other_it) return false;
    }
  return true;
}

void SliceType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _type->combine_hash(seed);
}

bool SliceType::equals_impl(const ExprType &other) const {
  return *_type == *static_cast<const SliceType&>(other).type();
}

void EnumType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<std::string_view> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
  // not rely on fields
  /*
  for(const auto &[name, type]: _variants) {
    combine_hash_impl(seed, static_cast<std::size_t>(hasher(name)));
    type->combine_hash(seed);
  }
  */
}

bool EnumType::equals_impl(const ExprType &other) const {
  const auto &other_struct = static_cast<const EnumType&>(other);
  if(_ident != other_struct.ident()) return false;
  // not rely on fields
  /*
  const auto &other_variants = other_struct.variants();
  if(_variants.size() != other_variants.size()) return false;
  for(auto it = _variants.begin(), other_it = other_variants.begin();
    other_it != other_variants.end(); ++it, ++other_it) {
    if(it->first != other_it->first) return false;
    if(*it->second != *other_it->second) return false;
  }
  */
  return true;
}

void FunctionType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<std::string_view> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
  for(const auto &type: _params)
    type->combine_hash(seed);
}

bool FunctionType::equals_impl(const ExprType &other) const {
  const auto &other_func = static_cast<const FunctionType&>(other);
  if(_ident != other_func.ident()) return false;
  const auto &other_params = other_func.params();
  if(_params.size() != other_params.size()) return false;
  for(auto it = _params.begin(), other_it = other_params.begin();
    it != _params.end(); ++it, ++other_it) {
    if(**it != **other_it) return false;
  }
  return true;
}

void TraitType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<std::string_view> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
}

bool TraitType::equals_impl(const ExprType &other) const {
  const auto &other_trait = static_cast<const TraitType&>(other);
  if(_ident != other_trait.ident()) return false;
  return true;
}

void RangeType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<std::string_view> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _type->combine_hash(seed);
}

bool RangeType::equals_impl(const ExprType &other) const {
  const auto &other_range = static_cast<const RangeType&>(other);
  return *_type == *other_range.type();
}



}
