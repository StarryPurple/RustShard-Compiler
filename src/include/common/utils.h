#ifndef RUST_SHARD_COMMON_UTILS_H
#define RUST_SHARD_COMMON_UTILS_H

#include <type_traits>
#include <variant>
#include <algorithm>

namespace insomnia::rust_shard::utils {

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
  char, std::string,
  std::int64_t,
  std::uint64_t,
  float, double,
  bool>;

using primitive_variant = primitives::as_variant;

template <typename T>
concept is_primitive = primitives::contains<std::decay_t<T>>;

inline std::string to_base62(uint64_t num) {
  static auto BASE62_CHARS = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  if (num == 0) return "0";
  std::string result;
  result.reserve(12);
  while (num > 0) {
    result += BASE62_CHARS[num % 62];
    num /= 62;
  }
  std::reverse(result.begin(), result.end());
  return result;
}

}

#endif // RUST_SHARD_COMMON_UTILS_H
