#ifndef INSOMNIA_AST_TYPE_H
#define INSOMNIA_AST_TYPE_H

#include <memory>
#include <sys/stat.h>

namespace insomnia::ast {

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
  bool operator==(const ExprType &other) const {
    if(!_underlying && !other._underlying) return true;
    if(!_underlying || !other._underlying) return false;
    return *_underlying == *other._underlying;
  }
  std::size_t hash() const;
private:
  std::shared_ptr<ExprType> _underlying;
};

// referred to boost::hash_combine
struct ExprTypeHash {
  std::size_t operator()(const ExprType &type) const {
    static constexpr std::size_t magic = 0x9e3779b9;
    auto ptr = type.get_underlying();
    std::size_t res = magic + static_cast<std::size_t>(type.get_kind());
    while(ptr) {
      res ^= magic + (res << 6) + (res >> 2) + static_cast<std::size_t>(ptr->get_kind());
      ptr = ptr->get_underlying();
    }
    return res;
  }
};

}

#endif // INSOMNIA_AST_TYPE_H