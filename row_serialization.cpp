#include "row_serialization.h"
#include "row.h"
#include <cstring>
#include <cstdint>

void serialize_row(const Row& source, void* destination) {
    uint32_t offset = 0;
    std::memcpy((char*)destination + offset, &source.is_deleted, sizeof(source.is_deleted));
    offset += sizeof(source.is_deleted);
    std::memcpy((char*)destination + offset, &source.id, sizeof(source.id));
    offset += sizeof(source.id);
    std::memcpy((char*)destination + offset, source.name, COLUMN_NAME_SIZE);
    offset += COLUMN_NAME_SIZE;
    std::memcpy((char*)destination + offset, source.email, COLUMN_EMAIL_SIZE);
}

void deserialize_row(const void* source, Row& destination) {
    uint32_t offset = 0;
    std::memcpy(&destination.is_deleted, (char*)source + offset, sizeof(destination.is_deleted));
    offset += sizeof(destination.is_deleted);
    std::memcpy(&destination.id, (char*)source + offset, sizeof(destination.id));
    offset += sizeof(destination.id);
    std::memcpy(destination.name, (char*)source + offset, COLUMN_NAME_SIZE);
    offset += COLUMN_NAME_SIZE;
    std::memcpy(destination.email, (char*)source + offset, COLUMN_EMAIL_SIZE);
}
