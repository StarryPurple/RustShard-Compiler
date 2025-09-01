#ifndef INSOMNIA_AST_TYPE_H
#define INSOMNIA_AST_TYPE_H

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_set>

#include "ast_type.h"
#include "ast_type.h"
#include "ast_type.h"

namespace insomnia::rust_shard::sem_type {

enum class TypeKind;
class ExprType;

// a wrapper, supporting dynamic cast from basic ExprType
struct TypePtr {
  std::shared_ptr<ExprType> ptr;

  TypePtr() = default;
  explicit TypePtr(std::shared_ptr<ExprType> p): ptr(std::move(p)) {}
  TypePtr(const TypePtr&) = default;
  TypePtr(TypePtr&&) = default;
  TypePtr& operator=(const TypePtr&) = default;
  TypePtr& operator=(TypePtr&&) = default;
  ~TypePtr() = default;

  bool operator==(const TypePtr &that) const;

  ExprType& operator*() { return *ptr.get(); }
  ExprType* operator->() { return ptr.get(); }
  const ExprType& operator*() const { return *ptr.get(); }
  const ExprType* operator->() const { return ptr.get(); }
  explicit operator bool() const { return static_cast<bool>(ptr); }

  template <class T> requires std::derived_from<T, ExprType>
  std::shared_ptr<T> get() const { return std::dynamic_pointer_cast<T>(ptr); }
};

enum class TypePrime {
  kChar, kBool,
  kI8, kI16, kI32, kI64,
  kU8, kU16, kU32, kU64,
  kISize, kUSize,
  kF32, kF64,
  kString
};

enum class TypeKind {
  kInvalid,
  kPrimitive,
  kArray,
  kReference,
  kStruct,
  kTuple,
  kSlice,
  kAlias, // redundant...
  kEnum,
  kFunction,
};

// referred to boost::hash_combine
// Heh, CRTP..., NVI...
// TODO: Add a pretty printer. Maybe in another language...
class ExprType : public std::enable_shared_from_this<ExprType> {
public:
  explicit ExprType(TypeKind kind) : _kind(kind) {}
  virtual ~ExprType() = default;
  TypeKind kind() const { return _kind; } // type of this layer
  bool operator==(const ExprType &other) const;
  bool operator!=(const ExprType &other) const { return !(*this == other); }
  virtual void combine_hash(std::size_t &seed) const = 0; // hash of this layer
  std::size_t hash() const; // calls this->combine_hash(seed = 0) and returns the seed.
protected:
  TypeKind _kind;

  virtual bool equals_impl(const ExprType &other) const = 0;
  static void combine_hash_impl(std::size_t &seed, std::size_t h);
private:
  TypePtr remove_alias() const;
};

class PrimitiveType : public ExprType {
public:
  explicit PrimitiveType(TypePrime prime)
  : ExprType(TypeKind::kPrimitive), _prime(prime) {}
  TypePrime prime() const { return _prime; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  TypePrime _prime;
};

class ArrayType : public ExprType {
public:
  ArrayType(TypePtr type, std::size_t length)
  : ExprType(TypeKind::kArray), _type(std::move(type)), _length(length) {}
  ArrayType(std::shared_ptr<ExprType> type, std::size_t length)
  : ExprType(TypeKind::kArray), _type(std::move(type)), _length(length) {}
  TypePtr type() const { return _type; }
  std::size_t length() const { return _length; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  TypePtr _type;
  std::size_t _length;
};

class ReferenceType : public ExprType {
public:
  explicit ReferenceType(TypePtr type)
  : ExprType(TypeKind::kReference), _type(std::move(type)) {}
  explicit ReferenceType(std::shared_ptr<ExprType> type)
  : ExprType(TypeKind::kReference), _type(std::move(type)) {}
  TypePtr type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  TypePtr _type;
};

class StructType : public ExprType {
public:
  explicit StructType(std::string_view ident)
  : ExprType(TypeKind::kStruct),
  _ident(std::move(ident)) {}
  void set_fields(std::map<std::string_view, TypePtr> &&fields) {
    _fields = std::move(fields);
  }
  std::string_view ident() const { return _ident; }
  const std::map<std::string_view, TypePtr>& fields() const { return _fields; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string_view _ident;
  std::map<std::string_view, TypePtr> _fields;
};

class TupleType : public ExprType {
public:
  explicit TupleType(std::vector<TypePtr> &&members)
  : ExprType(TypeKind::kTuple), _members(std::move(members)) {}
  const std::vector<TypePtr>& members() const { return _members; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::vector<TypePtr> _members;
};

class SliceType : public ExprType {
public:
  explicit SliceType(TypePtr type)
  : ExprType(TypeKind::kSlice), _type(std::move(type)) {}
  TypePtr type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  TypePtr _type;
};

class AliasType : public ExprType {
public:
  explicit AliasType(std::string_view ident)
  : ExprType(TypeKind::kAlias), _ident(std::move(ident)) {}
  void set_type(TypePtr type) { _type = std::move(type); }
  std::string_view ident() const { return _ident; }
  TypePtr type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string_view _ident;
  TypePtr _type;
};

class EnumType : public ExprType {
public:
  explicit EnumType(std::string_view ident)
  : ExprType(TypeKind::kEnum), _ident(std::move(ident)) {}
  std::string_view ident() const { return _ident; }
  void set_variants(std::map<std::string_view, TypePtr> &&variants) {
    _variants = std::move(variants);
  }
  const std::map<std::string_view, TypePtr>& variants() const { return _variants; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string_view _ident;
  std::map<std::string_view, TypePtr> _variants;
};

class FunctionType : public ExprType {
public:
  FunctionType(
    std::string ident,
    std::vector<TypePtr> &&params
  ): ExprType(TypeKind::kFunction), _ident(std::move(ident)),
  _params(std::move(params)) {}
  const std::string& ident() const { return _ident; }
  const std::vector<TypePtr>& params() const { return _params; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string _ident;
  std::vector<TypePtr> _params;
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
  TypePool();
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
  template <class T, class... Args>
  requires std::derived_from<T, ExprType> && std::is_constructible_v<T, Args...>
  TypePtr make_type(Args &&...args) {
    return TypePtr(make_raw_type<T>(std::forward<Args>(args)...));
  }
  TypePtr make_unit() {
    return make_type<TupleType>(std::vector<TypePtr>());
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

#endif // INSOMNIA_AST_TYPE_H