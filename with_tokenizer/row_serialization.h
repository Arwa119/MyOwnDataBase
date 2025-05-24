#ifndef ROW_SERIALIZATION_H
#define ROW_SERIALIZATION_H

#include "row.h"
#include <cstring>
#include <cstdint>

void serialize_row(const Row& source, void* destination);
void deserialize_row(const void* source, Row& destination);

#endif
