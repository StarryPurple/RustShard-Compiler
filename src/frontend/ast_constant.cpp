#include "ast_constant.h"

namespace insomnia::rust_shard::sconst {
bool ConstValPtr::operator==(const ConstValPtr &other) const {
  if(!_ptr || !other._ptr) return _ptr == other._ptr;
  return *_ptr == *other._ptr;
}

std::size_t ConstPool::ConstValueSharedPtrHash::operator()(const std::shared_ptr<ConstValue> &ptr) const {
  std::size_t type_hash = ptr->type()->hash();
  std::size_t value_hash = 0;
  std::visit([&]<typename T0>(const T0 &val) {
    using T = std::decay_t<T0>;
    if constexpr(std::is_same_v<T, ConstPrime>) {
      std::visit([&]<typename T1>(const T1& primitive_val) {
        value_hash = std::hash<std::decay_t<T1>>()(primitive_val);
      }, val.value);
    } else if constexpr(!std::is_same_v<T, std::monostate>) {
      // value_hash = std::hash<T>()(val);
    }
  }, ptr->const_val());
  return type_hash ^ value_hash;
}

}
