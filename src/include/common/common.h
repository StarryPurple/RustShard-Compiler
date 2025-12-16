#ifndef INSOMNIA_COMMON_H
#define INSOMNIA_COMMON_H

#include <string>

namespace insomnia::rust_shard {

#define UNIMPLEMENTED(message) \
  throw std::runtime_error(message);

using StringRef = std::string;

}

#endif // INSOMNIA_COMMON_H