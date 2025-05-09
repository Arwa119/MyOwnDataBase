#include "row.h"
#include <iostream>
#include <cstring>

void print_row(const Row& row) {
    std::cout << "(" << row.id << ", " << row.name << ", " << row.email << ")" << std::endl;
}

Row create_row(int id, const std::string& name, const std::string& email) {
    Row row{};
    row.is_deleted = false; // Initialize as not deleted
    row.id = id;
    std::strncpy(row.name, name.c_str(), COLUMN_NAME_SIZE - 1);
    row.name[COLUMN_NAME_SIZE - 1] = '\0';

    std::strncpy(row.email, email.c_str(), COLUMN_EMAIL_SIZE - 1);
    row.email[COLUMN_EMAIL_SIZE - 1] = '\0';
    return row;
}
