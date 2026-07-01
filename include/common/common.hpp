#ifndef RUST_SHARD_COMMON_H
#define RUST_SHARD_COMMON_H

#include <stdexcept>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

namespace rshard {

#define UNIMPLEMENTED(message) \
  throw std::runtime_error(message);

using StringT = std::string;

static constexpr bool kEnableVarHints = false;
static constexpr bool kEnableAsmComment = true;

namespace ast {
  using node_id_t = int;
}

namespace ir {
  using reg_id_t = int;
  using block_id_t = int;
  using hint_id_t = int;
}

using imm_val_t = std::int64_t;
using offset_t = std::int32_t;

static_assert(std::is_signed_v<imm_val_t>);
static_assert(std::is_signed_v<offset_t>);

struct Bitmap {
  std::vector<std::uint64_t> map;
  int width;

  explicit Bitmap(int n, bool val): map((n + 63) / 64), width(n) {
    if(val) {
      std::memset(map.data(), 0xff, map.size() * sizeof(std::uint64_t));
      if(n % 64 != 0) map.back() &= (1ull << (n % 64)) - 1;
    }
  }

  [[nodiscard]]
  bool get(int p) const {
    if(p < 0 || p >= width) {
      throw std::runtime_error("Unexpected usage");
    }
    return (map[p / 64] >> (p % 64)) & 1;
  }

  void set(int p, bool val) {
    if(p < 0 || p >= width) {
      throw std::runtime_error("Unexpected usage");
    }
    if(val) {
      map[p / 64] |= (1ull << (p % 64));
    } else {
      map[p / 64] &= ~(1ull << (p % 64));
    }
  }

  Bitmap operator|(const Bitmap& other) const {
    if(width != other.width) {
      throw std::runtime_error("Unexpected usage");
    }
    Bitmap res(width, false);
    for(int i = 0; i < map.size(); ++i)
      res.map[i] = map[i] | other.map[i];
    return res;
  }

  Bitmap operator&(const Bitmap& other) const {
    if(width != other.width) {
      throw std::runtime_error("Unexpected usage");
    }
    Bitmap res(width, false);
    for(int i = 0; i < map.size(); ++i)
      res.map[i] = map[i] & other.map[i];
    return res;
  }

  Bitmap& operator|=(const Bitmap& other) {
    if(width != other.width) {
      throw std::runtime_error("Unexpected usage");
    }
    for(int i = 0; i < map.size(); ++i)
      map[i] |= other.map[i];
    return *this;
  }

  Bitmap& operator&=(const Bitmap& other) {
    if(width != other.width) {
      throw std::runtime_error("Unexpected usage");
    }
    for(int i = 0; i < map.size(); ++i)
      map[i] &= other.map[i];
    return *this;
  }

  bool operator==(const Bitmap& other) const {
    return map == other.map;
  }

  bool operator!=(const Bitmap& other) const {
    return map != other.map;
  }

  [[nodiscard]] int popcount() const {
    int cnt = 0;
    for(auto w: map) cnt += __builtin_popcountll(w);
    return cnt;
  }

  template<typename F>
  void for_each(F&& fn) const {
    for(int i = 0; i < static_cast<int>(map.size()); ++i) {
      uint64_t w = map[i];
      while(w) {
        int bit = __builtin_ctzll(w);
        fn(i * 64 + bit);
        w &= w - 1;
      }
    }
  }
};

}

#endif // RUST_SHARD_COMMON_H