#ifndef DATAGEN_CLI_H
#define DATAGEN_CLI_H

#include "datagen.h"

namespace SHAYVERI::DatagenCLI {

bool parse_args(int argc, char **argv, DatagenOptions &options);
void print_usage(const char *argv0);

} // namespace SHAYVERI::DatagenCLI

#endif // DATAGEN_CLI_H
