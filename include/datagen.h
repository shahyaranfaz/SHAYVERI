#ifndef DATAGEN_H
#define DATAGEN_H

#include "types.h"

namespace SHAYVERI {

int generate_data(int threads, U64 target_positions, const char *output_prefix);

} // namespace SHAYVERI

#endif // DATAGEN_H
