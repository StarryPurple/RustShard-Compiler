#ifndef INSOMNIA_AST_TYPE_H
#define INSOMNIA_AST_TYPE_H

#include <map>
#include <memory>
#include <vector>

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
  CHAR, I8, I16, I32, I64, U8, U16, U32, U64, ISIZE, USIZE, F32, F64, BOOL,
  STRING // Not used in types. Used in LiteralExpression AST node.
};

enum class TypeKind {
  INVALID,
  PRIMITIVE,
  ARRAY,
  REFERENCE,
  STRUCT,
  TUPLE,
  SLICE,
  ALIAS, // redundant node for debug.
};

// referred to boost::hash_combine
class ExprType {
public:
  ExprType(bool is_mut, TypeKind kind) : _is_mut(is_mut), _kind(kind) {}
  virtual ~ExprType() = default;
  virtual TypeKind get_kind() const { return _kind; } // type of this layer
  bool operator==(const ExprType &other) const {
    if(_kind != other._kind) return false;
    return this->equals_impl(other);
  }
  bool operator!=(const ExprType &other) const {
    return !(*this == other);
  }
  virtual void combine_hash(std::size_t &seed) const = 0; // hash of this layer
protected:
  bool _is_mut;
  TypeKind _kind;
  virtual bool equals_impl(const ExprType &other) const = 0;
  static void combine_hash_impl(std::size_t &seed, std::size_t h) {
    static constexpr std::size_t HASH_MAGIC_NUM = 0x9e3779b9;
    seed ^= HASH_MAGIC_NUM + h + (seed << 6) + (seed >> 2);
  }
};

struct ExprTypeHash {
  std::size_t operator()(const ExprType &type) const {
    std::size_t seed = 0;
    type.combine_hash(seed);
    return seed;
  }
};



class PrimitiveType : public ExprType {
public:
  PrimitiveType(TypePrime prime, bool is_mut)
  : ExprType(is_mut, TypeKind::PRIMITIVE), _prime(prime) {}
  TypePrime get_prime() const { return _prime; }
  void combine_hash(std::size_t &seed) const override {
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    combine_hash_impl(seed, static_cast<std::size_t>(_prime));
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    return _prime == static_cast<const PrimitiveType&>(other).get_prime();
  }
private:
  TypePrime _prime;
};

class ArrayType : public ExprType {
public:
  ArrayType(std::shared_ptr<ExprType> type, std::size_t length, bool is_mut)
  : ExprType(is_mut, TypeKind::ARRAY), _type(type), _length(length) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  std::size_t length() const { return _length; }
  void combine_hash(std::size_t &seed) const override {
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    _type->combine_hash(seed);
    combine_hash_impl(seed, _length);
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    const auto &other_array = static_cast<const ArrayType&>(other);
    if(_length != other_array.length()) return false;
    return *_type == *other_array.get_type();
  }
private:
  std::shared_ptr<ExprType> _type;
  std::size_t _length;
};

class ReferenceType : public ExprType {
public:
  ReferenceType(std::shared_ptr<ExprType> type, bool is_mut)
  : ExprType(is_mut, TypeKind::REFERENCE), _type(type) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override {
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    _type->combine_hash(seed);
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    return *_type == *static_cast<const ReferenceType&>(other).get_type();
  }
private:
  std::shared_ptr<ExprType> _type;
};

class StructType : public ExprType {
public:
  StructType(
    std::string ident,
    std::map<std::string, std::shared_ptr<ExprType>> &&fields,
    bool is_mut
  ): ExprType(is_mut, TypeKind::STRUCT), _ident(std::move(ident)), _fields(std::move(fields)) {}
  const std::string& get_ident() const { return _ident; }
  const std::map<std::string, std::shared_ptr<ExprType>>& get_fields() const { return _fields; }
  void combine_hash(std::size_t &seed) const override {
    static constexpr std::hash<std::string> hasher;
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    combine_hash_impl(seed, static_cast<std::size_t>(hasher(_ident)));
    for(auto &[name, type]: _fields) {
      combine_hash_impl(seed, static_cast<std::size_t>(hasher(name)));
      type->combine_hash(seed);
    }
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    const auto &other_struct = static_cast<const StructType&>(other);
    if(_ident != other_struct.get_ident()) return false;
    const auto &other_fields = other_struct.get_fields();
    if(_fields.size() != other_fields.size()) return false;
    for(auto it = _fields.begin(), other_it = other_fields.begin();
      other_it != other_fields.end(); ++it, ++other_it) {
      if(it->first != other_it->first) return false;
      if(*it->second != *other_it->second) return false;
    }
    return true;
  }
private:
  std::string _ident;
  std::map<std::string, std::shared_ptr<ExprType>> _fields;
};

class TupleType : public ExprType {
public:
  TupleType(std::vector<std::shared_ptr<ExprType>> &&members, bool is_mut)
  : ExprType(is_mut, TypeKind::TUPLE), _members(std::move(members)) {}
  const std::vector<std::shared_ptr<ExprType>>& get_members() const { return _members; }
  void combine_hash(std::size_t &seed) const override {
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    for(auto &type: _members) type->combine_hash(seed);
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    const auto &other_tuple = static_cast<const TupleType&>(other);
    const auto &other_members = other_tuple.get_members();
    if(_members.size() != other_members.size()) return false;
    for(auto it = _members.begin(), other_it = other_members.begin();
      it != _members.end(); ++it, ++other_it) {
      if(**it != **other_it) return false;
    }
    return true;
  }
private:
  std::vector<std::shared_ptr<ExprType>> _members;
};

class SliceType : public ExprType {
public:
  SliceType(std::shared_ptr<ExprType> type, bool is_mut)
  : ExprType(is_mut, TypeKind::SLICE), _type(type) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override {
    combine_hash_impl(seed, static_cast<std::size_t>(_is_mut));
    combine_hash_impl(seed, static_cast<std::size_t>(_kind));
    _type->combine_hash(seed);
  }
protected:
  bool equals_impl(const ExprType &other) const override {
    return *_type == *static_cast<const SliceType&>(other).get_type();
  }
private:
  std::shared_ptr<ExprType> _type;
};

class AliasType : public ExprType {
public:
  explicit AliasType(std::shared_ptr<ExprType> type)
  : ExprType(false, TypeKind::ALIAS), _type(type) {}
  std::shared_ptr<ExprType> get_type() const { return _type; }
  void combine_hash(std::size_t &seed) const override {} // Do nothing.
protected:
  bool equals_impl(const ExprType &other) const override {
    return *_type == *static_cast<const AliasType&>(other).get_type();
  }
private:
  std::shared_ptr<ExprType> _type;
};

}

#endif // INSOMNIA_AST_TYPE_H