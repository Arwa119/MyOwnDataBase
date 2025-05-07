#ifndef ROW_SERIALIZATION_H
#define ROW_SERIALIZATION_H

#include "row.h"
#include <cstring>
#include <cstdint>

// Serialize Row -> to memory location
void serialize_row(const Row& source, void* destination) {
    memcpy(destination, &source.id, sizeof(source.id));
    memcpy((char*)destination + sizeof(source.id), &source.name, COLUMN_NAME_SIZE);
    memcpy((char*)destination + sizeof(source.id) + COLUMN_NAME_SIZE, &source.email, COLUMN_EMAIL_SIZE);
}

// Deserialize memory location -> to Row
void deserialize_row(const void* source, Row& destination) {
    memcpy(&destination.id, source, sizeof(destination.id));
    memcpy(&destination.name, (char*)source + sizeof(destination.id), COLUMN_NAME_SIZE);
    memcpy(&destination.email, (char*)source + sizeof(destination.id) + COLUMN_NAME_SIZE, COLUMN_EMAIL_SIZE);
}

#endif