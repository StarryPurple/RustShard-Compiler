#include "ast_type.h"

namespace insomnia::rust_shard::sem_type {

bool TypePtr::operator==(const TypePtr &that) const {
  if(!ptr || !that.ptr) return ptr == that.ptr;
  return *ptr == *that.ptr;
}

std::size_t ExprType::hash() const {
  std::size_t seed = 0;
  this->combine_hash(seed);
  return seed;
}

bool ExprType::operator==(const ExprType &other) const  {
  auto self_ptr = remove_alias();
  auto other_ptr = other.remove_alias();
  if(self_ptr->_kind != other_ptr->_kind) return false;
  return self_ptr->equals_impl(*other_ptr);
}

TypePtr ExprType::remove_alias() const {
  auto ptr = TypePtr(std::const_pointer_cast<ExprType>(shared_from_this()));
  while(ptr->kind() == TypeKind::kAlias) {
    ptr = ptr.get<AliasType>()->type();
  }
  return ptr;
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

void AliasType::combine_hash(std::size_t &seed) const {
  _type->combine_hash(seed);
  // Do nothing more. Type alias shouldn't affect the essence of the type.
}

bool AliasType::equals_impl(const ExprType &other) const {
  throw std::runtime_error("Compiler type error: Trying to check alias equality.");
  // code should not reach here. But, just in case...
  return *_type == *static_cast<const AliasType&>(other).type();
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
  static constexpr std::hash<std::string> hasher;
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

TypePool::TypePool() {
  // register all primitive types first.
  static const std::vector<TypePrime> primes = {
    TypePrime::kBool, TypePrime::kChar,
    TypePrime::kI8, TypePrime::kI16, TypePrime::kI32, TypePrime::kI64,
    TypePrime::kU8, TypePrime::kU16, TypePrime::kU32, TypePrime::kU64,
    TypePrime::kISize, TypePrime::kUSize,
    TypePrime::kF32, TypePrime::kF64,
    TypePrime::kString
  };
  for(auto prime: primes) {
    _pool.emplace(std::make_shared<PrimitiveType>(prime));
  }
  // unit type
  _pool.emplace(std::make_shared<TupleType>(std::vector<TypePtr>()));
}


}