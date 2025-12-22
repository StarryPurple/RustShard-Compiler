#include "type.h"

#include <unordered_map>

namespace insomnia::rust_shard::stype {

StringRef prime_strs(TypePrime prime) {
  static const std::unordered_map<TypePrime, StringRef> table = {
    {TypePrime::kChar, "char"}, {TypePrime::kBool, "bool"},
    {TypePrime::kI8, "i8"}, {TypePrime::kI16, "i16"},
    {TypePrime::kI32, "i32"}, {TypePrime::kI64, "i64"},
    {TypePrime::kU8, "u8"}, {TypePrime::kU16, "u16"},
    {TypePrime::kU32, "u32"}, {TypePrime::kU64, "u64"},
    {TypePrime::kISize, "isize"}, {TypePrime::kUSize, "usize"},
    {TypePrime::kF32, "f32"}, {TypePrime::kF64, "f64"},
    {TypePrime::kString, "str"}, {TypePrime::kXInt, "Xint"},
    {TypePrime::kXFloat, "Xfloat"}
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
  }; // no Xint and Xfloat
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

const ExprType* ExprType::remove_alias() const {
  auto current = this;
  while(current->kind() == TypeKind::kAlias)
    current = static_cast<const AliasType *>(current);
  return current;
}

bool ExprType::operator==(const ExprType &other) const {
  auto lhs = remove_alias(), rhs = other.remove_alias();
  if(lhs->kind() != rhs->kind()) return false;
  return lhs->equals_impl(*rhs);
}

void ExprType::combine_hash_impl(std::size_t &seed, std::size_t h) {
  static constexpr std::size_t HASH_MAGIC_NUM = 0x9e3779b9;
  seed ^= HASH_MAGIC_NUM + h + (seed << 6) + (seed >> 2);
}

void PrimeType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(_prime));
}

bool PrimeType::equals_impl(const ExprType &other) const {
  return _prime == static_cast<const PrimeType&>(other).prime();
}

std::string PrimeType::to_string() const {
  switch(_prime) {
  case TypePrime::kBool: return "bool";
  case TypePrime::kChar: return "char";
  case TypePrime::kI8: return "i8";
  case TypePrime::kI16: return "i16";
  case TypePrime::kI32: return "i32";
  case TypePrime::kI64: return "i64";
  case TypePrime::kU8: return "u8";
  case TypePrime::kU16: return "u16";
  case TypePrime::kU32: return "u32";
  case TypePrime::kU64: return "u64";
  case TypePrime::kISize: return "isize";
  case TypePrime::kUSize: return "usize";
  case TypePrime::kF32: return "f32";
  case TypePrime::kF64: return "f64";
  case TypePrime::kString: return "String";
  }
  return "unrecognized prime";
}

void ArrayType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _inner->combine_hash(seed);
  combine_hash_impl(seed, _length);
}

bool ArrayType::equals_impl(const ExprType &other) const {
  const auto &other_array = static_cast<const ArrayType&>(other);
  if(_length != other_array.length()) return false;
  return *_inner == *other_array.inner();
}

std::string ArrayType::to_string() const {
  std::string res = "[";
  res += _inner->to_string();
  res += "; ";
  res += std::to_string(_length);
  res += "]";
  return res;
}

void RefType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(_ref_is_mut));
  _inner->combine_hash(seed);
}

bool RefType::equals_impl(const ExprType &other) const {
  const auto &other_ref = static_cast<const RefType&>(other);
  if(_ref_is_mut != other_ref.ref_is_mut()) return false;
  if(*_inner != *other_ref.inner()) return false;
  return true;
}

std::string RefType::to_string() const {
  std::string res = "&";
  if(_ref_is_mut) res += "mut ";
  res += _inner->to_string();
  return res;
}

void StructType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<StringRef> hasher;
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

std::string StructType::to_string() const {
  return _ident;
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

std::string TupleType::to_string() const {
  std::string res = "(";
  for(int i = 0; i < _members.size(); ++i) {
    res += _members[i]->to_string();
    if(i != _members.size() - 1) res += ", ";
  }
  if(_members.size() == 1) res += ",";
  res += ")";
  return res;
}

void SliceType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _inner->combine_hash(seed);
}

bool SliceType::equals_impl(const ExprType &other) const {
  return *_inner == *static_cast<const SliceType&>(other).inner();
}

std::string SliceType::to_string() const {
  std::string res = "[";
  res += _inner->to_string();
  res += "]";
  return res;
}

void EnumType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<StringRef> hasher;
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
  return _ident == other_struct.ident();
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
}

std::string EnumType::to_string() const {
  return _ident;
}

void FunctionType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<StringRef> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
  for(const auto &type: _params)
    type->combine_hash(seed);
  _ret_type->combine_hash(seed);
}

bool FunctionType::equals_impl(const ExprType &other) const {
  const auto &other_func = static_cast<const FunctionType&>(other);
  if(_ident != other_func.ident()) return false;
  if(_ret_type != other_func._ret_type) return false;
  const auto &other_params = other_func.params();
  if(_params.size() != other_params.size()) return false;
  for(auto it = _params.begin(), other_it = other_params.begin();
    it != _params.end(); ++it, ++other_it) {
    if(**it != **other_it) return false;
  }
  return true;
}

std::string FunctionType::to_string() const {
  std::string res = "fn(";
  for(int i = 0; i < _params.size(); ++i) {
    res += _params[i]->to_string();
    if(i != _params.size() - 1) res += ", ";
  }
  res += ") -> ";
  res += _ret_type->to_string();
  return res;
}

void TraitType::combine_hash(std::size_t &seed) const {
  static constexpr std::hash<StringRef> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
}

bool TraitType::equals_impl(const ExprType &other) const {
  const auto &other_trait = static_cast<const TraitType&>(other);
  return _ident == other_trait.ident();
}

std::string TraitType::to_string() const {
  return _ident;
}

void RangeType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  _type->combine_hash(seed);
}

bool RangeType::equals_impl(const ExprType &other) const {
  const auto &other_range = static_cast<const RangeType&>(other);
  return *_type == *other_range.type();
}

std::string RangeType::to_string() const {
  std::string res = "Range<";
  res += _type->to_string();
  res += ">";
  return res;
}

void EnumVariantType::combine_hash(std::size_t &seed) const {
  parent_enum()->combine_hash(seed);
  static constexpr std::hash<StringRef> hasher;
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
  combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
}

bool EnumVariantType::equals_impl(const ExprType &other) const {
  const auto other_ev = static_cast<const EnumVariantType&>(other);
  return *parent_enum() == *other_ev.parent_enum() && _ident == other_ev.ident();
}

std::string EnumVariantType::to_string() const {
  std::string res = _parent_enum->to_string();
  res += "::";
  res += _ident;
  return res;
}

void AliasType::combine_hash(std::size_t &seed) const {
  // do not let this layer affect anything
  _type->combine_hash(seed);
}

bool AliasType::equals_impl(const ExprType &other) const {
  throw std::runtime_error("ast type system error: comparing align types");
}

std::string AliasType::to_string() const {
  std::string res = _ident;
  res += "{a.k.a. ";
  res += _type->to_string();
  res += "}";
  return res;
}

void NeverType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
}

bool NeverType::equals_impl(const ExprType &other) const {
  return true;
}

std::string NeverType::to_string() const {
  return "!";
}

void SelfType::combine_hash(std::size_t &seed) const {
  combine_hash_impl(seed, static_cast<std::size_t>(_kind));
}

bool SelfType::equals_impl(const ExprType &other) const {
  const auto &other_self = static_cast<const SelfType&>(other);
  return true;
}

std::string SelfType::to_string() const {
  return "Self";
}

}
