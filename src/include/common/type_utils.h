#ifndef INSOMNIA_TYPE_UTILS_H
#define INSOMNIA_TYPE_UTILS_H

#include <type_traits>
#include <variant>

namespace insomnia::rust_shard::type_utils {

template <typename T, typename... Ts>
concept is_one_of = (std::is_same_v<T, Ts> || ...);

template <typename... Ts>
struct type_list {
  using as_variant = std::variant<Ts...>;

  template <typename T>
  static constexpr bool contains = is_one_of<T, Ts...>;
};

using primitives = type_list<
  char, std::string_view,
  std::int64_t,
  std::uint64_t,
  float, double,
  bool>;

using primitive_variant = primitives::as_variant;

template <typename T>
concept is_primitive = primitives::contains<std::decay_t<T>>;

}

#endif // INSOMNIA_TYPE_UTILS_H
