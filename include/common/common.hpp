#ifndef RUST_SHARD_COMMON_H
#define RUST_SHARD_COMMON_H

#include <cstdint>
#include <stdexcept>
#include <string>

namespace rshard {

#define UNIMPLEMENTED(message) \
  throw std::runtime_error(message);

using StringT = std::string;

static constexpr bool kEnableVarHints = false;

namespace ast {
  using node_id_t = int;
}

namespace ir {
  using reg_id_t = int;
  using block_id_t = int;
  using hint_id_t = int;
}
}

#endif // RUST_SHARD_COMMON_H