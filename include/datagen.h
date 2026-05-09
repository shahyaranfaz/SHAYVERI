#ifndef DATAGEN_H
#define DATAGEN_H

#include "types.h"

namespace SHAYVERI {

int generate_data(int threads, U64 target_positions, const char *output_prefix, U64 search_nodes);

} // namespace SHAYVERI

#endif // DATAGEN_H
