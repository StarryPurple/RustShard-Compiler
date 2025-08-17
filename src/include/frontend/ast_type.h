#ifndef INSOMNIA_AST_TYPE_H
#define INSOMNIA_AST_TYPE_H

#include <memory>
#include <sys/stat.h>

namespace insomnia::rust_shard::type {

enum class TypeKind;
class ExprType;
class ExprPrimitiveType;
class ExprArrayType;


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

class ExprType {
public:
  virtual ~ExprType() = default;
  virtual TypeKind get_kind() const = 0; // type of this layer
  std::shared_ptr<ExprType> get_underlying() const { return _underlying; }
  virtual bool operator==(const ExprType &other) const;
  virtual std::size_t hash() const = 0;
protected:
  std::shared_ptr<ExprType> _underlying;
};

// referred to boost::hash_combine
struct ExprTypeHash {
  std::size_t operator()(const ExprType &type) const {
    return type.hash();
  }
};

}

#endif // INSOMNIA_AST_TYPE_H