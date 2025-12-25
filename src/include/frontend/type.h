#ifndef RUST_SHARD_TYPE_H
#define RUST_SHARD_TYPE_H

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include <unordered_set>

#include "type.h"
#include "common.h"

namespace insomnia::rust_shard::stype {

enum class TypeKind;
class ExprType;

using usize_t = std::uint64_t;

// a wrapper, supporting dynamic cast from basic ExprType
class TypePtr {
  std::shared_ptr<ExprType> _ptr;
public:

  struct Hash { size_t operator()(const TypePtr &tp) const; };
  struct Equal { bool operator()(const TypePtr &A, const TypePtr &B) const; };

  TypePtr() = default;
  explicit TypePtr(std::shared_ptr<ExprType> p): _ptr(std::move(p)) {}
  TypePtr(const TypePtr &) = default;
  TypePtr(TypePtr &&) noexcept = default;
  TypePtr& operator=(const TypePtr &) = default;
  TypePtr& operator=(TypePtr &&) noexcept = default;
  ~TypePtr() = default;

  bool operator==(const TypePtr &other) const;

  ExprType& operator*() { return *_ptr; }
  ExprType* operator->() { return _ptr.get(); }
  const ExprType& operator*() const { return *_ptr; }
  const ExprType* operator->() const { return _ptr.get(); }
  explicit operator bool() const { return static_cast<bool>(_ptr); }

  // uses static_pointer_cast. use it only when you have confirmed its inner type.
  template <class T> requires std::derived_from<T, ExprType>
  std::shared_ptr<T> get() const { return std::static_pointer_cast<T>(_ptr); }

  // uses dynamic_pointer_cast.
  template <class T> requires std::derived_from<T, ExprType>
  std::shared_ptr<T> get_if() const { return std::dynamic_pointer_cast<T>(_ptr); } // NOLINT

};

struct TypePath {
  std::vector<StringRef> segments;
  bool is_absolute;
};

enum class TypePrime {
  kChar, kBool,
  kI8, kI16, kI32, kI64, kISize, // order related with PrimitiveType.
  kU8, kU16, kU32, kU64, kUSize, // order related with PrimitiveType.
  kF32, kF64,
  kString,
  kInt, kFloat, // undetermined types: Integer, floating point.
};

StringRef prime_strs(TypePrime prime);
const std::vector<TypePrime>& type_primes();

enum class TypeKind {
  kInvalid,
  kPrimitive,
  kArray,
  kRef,
  kStruct,
  kTuple,
  kSlice,
  kEnum,
  kFunction,
  kTrait,
  kRange,
  kEnumVariant,
  kAlias,
  kNever,
  kSelf
};

// referred to boost::hash_combine
// Heh, CRTP..., NVI...
// Add a pretty printer. Maybe in another language...
// Added/.
class ExprType : public std::enable_shared_from_this<ExprType> {
public:
  explicit ExprType(TypeKind kind) : _kind(kind) {}
  virtual ~ExprType() = default;

  TypeKind kind() const { return _kind; } // type of this layer

  bool operator==(const ExprType &other) const {
    auto lhs = remove_alias(), rhs = other.remove_alias();
    if(lhs->kind() != rhs->kind()) return false;
    return lhs->equals_impl(*rhs);
  }

  bool operator!=(const ExprType &other) const { return !(*this == other); }

  virtual void combine_hash(std::size_t &seed) const = 0; // hash of this layer

  // calls this->combine_hash(seed = 0) and returns the seed.
  std::size_t hash() const {
    std::size_t seed = 0;
    this->combine_hash(seed);
    return seed;
  }

  virtual std::string to_string() const = 0;

  // whether this_tp <- from_tp is allowed.
  // 1. tt = ft
  // 2. i32... <- NatI, u32... <- NatI, i32... <- NegI, f32... <- Float
  // 3. recursive
  bool is_convertible_from(const ExprType &other) const {
    auto lhs = remove_alias(), rhs = other.remove_alias();
    if(lhs->kind() != rhs->kind()) return false;
    return lhs->convertible_impl(*rhs);
  }
  // whether this_tp <- from_tp is allowed.
  // Alongside the 3 conditions of is_convertible_from, we also allow:
  // 4. from_tp == NeverType.
  bool is_coercible_from(const ExprType &other) const {
    auto lhs = remove_alias(), rhs = other.remove_alias();
    if(rhs->kind() == TypeKind::kNever) return true;
    if(lhs->kind() != rhs->kind()) return false;
    return lhs->convertible_impl(*rhs);
  }
protected:
  TypeKind _kind;

  virtual bool equals_impl(const ExprType &other) const = 0;
  virtual bool convertible_impl(const ExprType &other) const = 0;
  static void combine_hash_impl(std::size_t &seed, std::size_t h);
private:
  const ExprType* remove_alias() const;
};

class PrimeType : public ExprType {
public:
  explicit PrimeType(TypePrime prime)
  : ExprType(TypeKind::kPrimitive), _prime(prime) {}
  TypePrime prime() const { return _prime; }
  bool is_number() const {
    return _prime != TypePrime::kBool && _prime != TypePrime::kChar && _prime != TypePrime::kString;
  }
  bool is_integer() const {
    return is_signed() || is_unsigned();
  }
  bool is_float() const {
    return _prime == TypePrime::kF32 || _prime == TypePrime::kF64;
  }
  bool is_signed() const {
    return _prime == TypePrime::kI8 || _prime == TypePrime::kI16 || _prime == TypePrime::kI32
      || _prime == TypePrime::kI64 || _prime == TypePrime::kISize;
  }
  bool is_unsigned() const {
    return _prime == TypePrime::kU8 || _prime == TypePrime::kU16 || _prime == TypePrime::kU32
      || _prime == TypePrime::kU64 || _prime == TypePrime::kUSize;
  }
  bool is_undetermined() const {
    return _prime == TypePrime::kInt || _prime == TypePrime::kFloat;
  }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePrime _prime;
};

class ArrayType : public ExprType {
public:
  ArrayType(TypePtr inner, usize_t length)
  : ExprType(TypeKind::kArray), _inner(std::move(inner)), _length(length) {}
  TypePtr inner() const { return _inner; }
  usize_t length() const { return _length; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePtr _inner;
  usize_t _length;
};

class RefType : public ExprType {
public:
  explicit RefType(TypePtr inner, bool ref_is_mut)
  : ExprType(TypeKind::kRef), _inner(std::move(inner)), _ref_is_mut(ref_is_mut) {}
  TypePtr inner() const { return _inner; }
  bool ref_is_mut() const { return _ref_is_mut; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePtr _inner;
  bool _ref_is_mut;
};

class StructType : public ExprType {
public:
  explicit StructType(StringRef ident)
  : ExprType(TypeKind::kStruct),
  _ident(std::move(ident)) {}
  void set_fields(std::map<StringRef, TypePtr> &&fields) {
    _fields = std::move(fields);
  }
  StringRef ident() const { return _ident; }
  const std::map<StringRef, TypePtr>& fields() const { return _fields; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  std::map<StringRef, TypePtr> _fields;
};

class TupleType : public ExprType {
public:
  explicit TupleType(std::vector<TypePtr> &&members)
  : ExprType(TypeKind::kTuple), _members(std::move(members)) {}
  const std::vector<TypePtr>& members() const { return _members; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  std::vector<TypePtr> _members;
};

class SliceType : public ExprType {
public:
  explicit SliceType(TypePtr inner)
  : ExprType(TypeKind::kSlice), _inner(std::move(inner)) {}
  TypePtr inner() const { return _inner; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePtr _inner;
};

class EnumVariantType;

class EnumType : public ExprType {
public:
  using variant_map_t = std::unordered_map<StringRef, std::shared_ptr<EnumVariantType>>;

  explicit EnumType(StringRef ident)
  : ExprType(TypeKind::kEnum), _ident(std::move(ident)) {}
  StringRef ident() const { return _ident; }
  void set_details(variant_map_t &&variants) {
    _variants = std::move(variants);
  }
  const variant_map_t& variants() const {
    return _variants;
  }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  variant_map_t _variants;
};

class FunctionType : public ExprType {
public:
  FunctionType(
    StringRef ident,
    TypePtr self_type_opt,
    std::vector<TypePtr> &&params,
    TypePtr ret_type
  ): ExprType(TypeKind::kFunction), _ident(std::move(ident)),
  _params(std::move(params)), _ret_type(std::move(ret_type)), _self_type_opt(std::move(self_type_opt)) {}
  StringRef ident() const { return _ident; }
  const std::vector<TypePtr>& params() const { return _params; }
  TypePtr ret_type() const { return _ret_type; }
  TypePtr self_type_opt() const { return _self_type_opt; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  std::vector<TypePtr> _params;
  TypePtr _ret_type;
  TypePtr _self_type_opt;
};

class TraitType : public ExprType {
public:
  using asso_func_map_t = std::unordered_map<StringRef, TypePtr>;
  using asso_type_map_t = std::unordered_map<StringRef, TypePtr>;
  using asso_const_map_t = std::unordered_map<StringRef, TypePtr>;

  explicit TraitType(StringRef ident)
  : ExprType(TypeKind::kTrait), _ident(std::move(ident)) {}
  void add_asso_func(const StringRef &ident, const TypePtr &asso_func) {
    auto f = asso_func.get_if<FunctionType>();
    if(!f) {
      throw std::runtime_error("TraitType: Not a trait function");
    }
    _asso_funcs.emplace(ident, std::move(f));
  }
  void add_asso_const(const StringRef &ident, const TypePtr &asso_const) {
    _asso_consts.emplace(ident, asso_const);
  }
  void add_asso_type(const StringRef &ident, const TypePtr &asso_type) {
    _asso_types.emplace(ident, asso_type);
  }
  StringRef ident() const { return _ident; }
  const std::unordered_map<StringRef, TypePtr>& asso_funcs() const {
    return _asso_funcs;
  }
  const std::unordered_map<StringRef, TypePtr>& asso_types() const {
    return _asso_types;
  }
  const std::unordered_map<StringRef, TypePtr>& asso_consts() const {
    return _asso_consts;
  }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  std::unordered_map<StringRef, TypePtr> _asso_funcs, _asso_types, _asso_consts;
};

class RangeType : public ExprType {
public:
  explicit RangeType(TypePtr type)
  : ExprType(TypeKind::kRange), _inner(std::move(type)) {}
  TypePtr inner() const { return _inner; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePtr _inner;
};

class EnumVariantType : public ExprType {
public:
  using discriminant_t = std::int64_t; // the actual type is seen in parent_enum->dis_type
  EnumVariantType(
    StringRef ident,
    discriminant_t discriminant,
    std::vector<TypePtr> &&asso_types,
    std::shared_ptr<EnumType> parent_enum
  ): ExprType(TypeKind::kEnumVariant), _ident(std::move(ident)), _discriminant(discriminant),
  _asso_types(std::move(asso_types)), _parent_enum(std::move(parent_enum)) {}
  StringRef ident() const { return _ident; }
  discriminant_t discriminant() const { return _discriminant; }
  const std::vector<TypePtr>& asso_types() const { return _asso_types; }
  std::shared_ptr<EnumType> parent_enum() const { return _parent_enum; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  discriminant_t _discriminant;
  std::vector<TypePtr> _asso_types;
  std::shared_ptr<EnumType> _parent_enum;
};

class AliasType : public ExprType {
public:
  explicit AliasType(
    StringRef ident
  ): ExprType(TypeKind::kAlias), _ident(std::move(ident)) {}
  void set_type(TypePtr type) { _type = std::move(type); }
  StringRef ident() const { return _ident; }
  TypePtr type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  StringRef _ident;
  TypePtr _type;
};

class NeverType : public ExprType {
public:
  NeverType(): ExprType(TypeKind::kNever) {}
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
};

class SelfType : public ExprType {
public:
  explicit SelfType(TypePtr inner)
  : ExprType(TypeKind::kSelf), _inner(std::move(inner)) {}
  TypePtr inner() const { return _inner; }
  bool has_inner() const { return static_cast<bool>(_inner); }
  void set_inner(TypePtr inner) { _inner = std::move(inner); }
  void combine_hash(std::size_t &seed) const override;
  std::string to_string() const override;
protected:
  bool equals_impl(const ExprType &other) const override;
  bool convertible_impl(const ExprType &other) const override;
private:
  TypePtr _inner;
};

class TypePool {
  struct ExprTypeSharedPtrHash {
    std::size_t operator()(const std::shared_ptr<ExprType> &obj) const {
      return obj->hash();
    }
  };

  struct ExprTypeSharedPtrEqual {
    bool operator()(
      const std::shared_ptr<ExprType> &A,
      const std::shared_ptr<ExprType> &B
    ) const { return *A == *B; }
  };
public:
  TypePool() = default;
  template <class T, class... Args>
  requires std::derived_from<T, ExprType> && std::is_constructible_v<T, Args...>
  std::shared_ptr<T> make_raw_type(Args &&...args) {
    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    auto it = _pool.find(ptr);
    if(it != _pool.end())
      return std::static_pointer_cast<T>(*it); // logically confirmed
    _pool.insert(ptr);
    return ptr;
  }
  // In fact, I shall list all possibilities... Never mind.
  template <class T, class... Args>
  requires std::derived_from<T, ExprType> && std::is_constructible_v<T, Args...>
  TypePtr make_type(Args &&...args) {
    return TypePtr(make_raw_type<T>(std::forward<Args>(args)...));
  }
  TypePtr make_unit() {
    return make_type<TupleType>(std::vector<TypePtr>());
  }
  TypePtr make_never() {
    return make_type<NeverType>();
  }

  std::size_t size() const { return _pool.size(); }
private:
  std::unordered_set<
    std::shared_ptr<ExprType>,
    ExprTypeSharedPtrHash,
    ExprTypeSharedPtrEqual
  > _pool;
};

}

#endif // RUST_SHARD_TYPE_H