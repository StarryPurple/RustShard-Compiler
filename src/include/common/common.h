#ifndef RUST_SHARD_COMMON_H
#define RUST_SHARD_COMMON_H

#include <string>

namespace insomnia::rust_shard {

#define UNIMPLEMENTED(message) \
  throw std::runtime_error(message);

using StringT = std::string;

}

#endif // RUST_SHARD_COMMON_H