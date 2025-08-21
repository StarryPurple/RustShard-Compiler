#ifndef INSOMNIA_AST_TYPE_H
#define INSOMNIA_AST_TYPE_H

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <unordered_set>

#include "ast_type.h"
#include "ast_type.h"

namespace insomnia::rust_shard::type {

enum class TypeKind;
class ExprType;
class PrimitiveType;
class ArrayType;
class ReferenceType;
class StructType;
class TupleType;
class SliceType;
class AliasType;

enum class TypePrime {
  kChar, kI8, kI16, kI32, kI64, kU8, kU16, kU32, kU64, kISize, kUSize, kF32, kF64, kBool,
  kString // Not used in types. Only be used in LiteralExpression AST node.
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
};

// referred to boost::hash_combine
// Heh, CRTP..., NVI...
// TODO: Add a pretty printer. Maybe in another language...
class ExprType : public std::enable_shared_from_this<ExprType> {
public:
  ExprType(bool is_mut, TypeKind kind) : _is_mut(is_mut), _kind(kind) {}
  virtual ~ExprType() = default;
  TypeKind get_kind() const { return _kind; } // type of this layer
  bool is_mut() const { return _is_mut; }
  bool operator==(const ExprType &other) const;
  bool operator!=(const ExprType &other) const { return !(*this == other); }
  virtual void combine_hash(std::size_t &seed) const = 0; // hash of this layer
  std::size_t hash() const; // calls this->combine_hash(seed = 0) and returns the seed.
protected:
  bool _is_mut;
  TypeKind _kind;

  virtual bool equals_impl(const ExprType &other) const = 0;
  static void combine_hash_impl(std::size_t &seed, std::size_t h);
private:
  std::shared_ptr<ExprType> remove_alias() const;
};

class PrimitiveType : public ExprType {
public:
  PrimitiveType(TypePrime prime, bool is_mut)
  : ExprType(is_mut, TypeKind::kPrimitive), _prime(prime) {}
  TypePrime get_prime() const { return _prime; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  TypePrime _prime;
};

class ArrayType : public ExprType {
public:
  ArrayType(std::shared_ptr<ExprType> type, std::size_t length, bool is_mut)
  : ExprType(is_mut, TypeKind::kArray), _type(std::move(type)), _length(length) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  std::size_t length() const { return _length; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::shared_ptr<ExprType> _type;
  std::size_t _length;
};

class ReferenceType : public ExprType {
public:
  ReferenceType(std::shared_ptr<ExprType> type, bool is_mut)
  : ExprType(is_mut, TypeKind::kReference), _type(std::move(type)) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::shared_ptr<ExprType> _type;
};

class StructType : public ExprType {
public:
  StructType(
    std::string ident,
    std::map<std::string, std::shared_ptr<ExprType>> &&fields,
    bool is_mut
  ): ExprType(is_mut, TypeKind::kStruct),
  _ident(std::move(ident)), _fields(std::move(fields)) {}
  const std::string& get_ident() const { return _ident; }
  const std::map<
    std::string,
    std::shared_ptr<ExprType>
  >& get_fields() const { return _fields; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string _ident;
  std::map<std::string, std::shared_ptr<ExprType>> _fields;
};

class TupleType : public ExprType {
public:
  TupleType(std::vector<std::shared_ptr<ExprType>> &&members, bool is_mut)
  : ExprType(is_mut, TypeKind::kTuple), _members(std::move(members)) {}
  const std::vector<std::shared_ptr<ExprType>>& get_members() const { return _members; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::vector<std::shared_ptr<ExprType>> _members;
};

class SliceType : public ExprType {
public:
  SliceType(std::shared_ptr<ExprType> type, bool is_mut)
  : ExprType(is_mut, TypeKind::kSlice), _type(std::move(type)) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::shared_ptr<ExprType> _type;
};

class AliasType : public ExprType {
public:
  explicit AliasType(std::string ident, std::shared_ptr<ExprType> type)
  : ExprType(false, TypeKind::kAlias), _ident(std::move(ident)), _type(std::move(type)) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string _ident;
  std::shared_ptr<ExprType> _type;
};

class EnumType : public ExprType {
public:
  EnumType(
    std::string ident,
    std::map<std::string, std::shared_ptr<ExprType>> &&variants,
    bool is_mut
  ): ExprType(is_mut, TypeKind::kEnum), _ident(std::move(ident)),
  _variants(std::move(variants)) {}
  const std::string& get_ident() const { return _ident; }
  const std::map<
    std::string,
    std::shared_ptr<ExprType>
  >& get_variants() const { return _variants; }
  void combine_hash(std::size_t &seed) const override;
protected:
  bool equals_impl(const ExprType &other) const override;
private:
  std::string _ident;
  std::map<std::string, std::shared_ptr<ExprType>> _variants;
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
  template <class T, class... Args> requires std::derived_from<T, ExprType>
  std::shared_ptr<T> make_type(Args &&...args) {
    auto ptr = std::make_shared<T>(std::forward<Args>(args)...);
    auto it = _pool.find(ptr);
    if(it != _pool.end())
      return std::static_pointer_cast<T>(*it);
    _pool.insert(ptr);
    return ptr;
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