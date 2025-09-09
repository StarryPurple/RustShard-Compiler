#ifndef INSOMNIA_COMMON_H
#define INSOMNIA_COMMON_H

namespace insomnia::rust_shard {

#define UNIMPLEMENTED(message) \
  throw std::runtime_error(message);

}

#endif // INSOMNIA_COMMON_H