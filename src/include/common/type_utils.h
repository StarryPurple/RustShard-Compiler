#ifndef INSOMNIA_TYPE_UTILS_H
#define INSOMNIA_TYPE_UTILS_H

#include <type_traits>
#include <variant>

namespace insomnia::rust_shard::type_utils {

template <typename T, typename... Ts>
concept is_one_of = (std::is_same_v<T, Ts> || ...);

template <typename... Ts>
struct type_list {
  template <template <typename...> class T>
  using to_template = T<Ts...>;

  using as_variant = to_template<std::variant>;

  template <typename T>
  static constexpr bool contains = is_one_of<T, Ts...>;

  static constexpr std::size_t size = sizeof...(Ts);

  template <typename T>
  using append = type_list<Ts..., T>;

  template <typename T>
  using prepend = type_list<T, Ts...>;

  template <typename... Us>
  using append_list = type_list<Ts..., Us...>;

  template <typename... Us>
  using prepend_list = type_list<Us..., Ts...>;

  template <std::size_t N>
  using at = std::tuple_element_t<N, std::tuple<Ts...>>;
};

using primitives = type_list<
  char, StringRef,
  std::int64_t,
  std::uint64_t,
  float, double,
  bool>;

using primitive_variant = primitives::as_variant;

template <typename T>
concept is_primitive = primitives::contains<std::decay_t<T>>;

}

#endif // INSOMNIA_TYPE_UTILS_H
