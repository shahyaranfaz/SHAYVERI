#ifndef UCI_OUTPUT_H
#define UCI_OUTPUT_H

#include <mutex>

namespace SHAYVERI {

// Search completion, iterative search information, and command responses may
// originate on different threads. Serialize complete protocol messages so
// their bytes cannot be interleaved on stdout.
inline std::mutex uci_output_mutex;

} // namespace SHAYVERI

#endif // UCI_OUTPUT_H
