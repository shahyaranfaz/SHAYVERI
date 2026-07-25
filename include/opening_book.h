#ifndef OPENING_BOOK_H
#define OPENING_BOOK_H

#include "types.h"

namespace SHAYVERI {

struct BookEntry {
    U64 key;
    char move[6];
};

const BookEntry *probe_book(U64 key);

} // namespace SHAYVERI

#endif // OPENING_BOOK_H
