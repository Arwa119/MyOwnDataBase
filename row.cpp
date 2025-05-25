#include "row.h"
#include <iostream>
#include <string>

Row create_row(int id, const std::string& name, const std::string& email) {
    Row row;
    row.id = id;
    row.is_deleted = false;
    std::strncpy(row.name, name.c_str(), COLUMN_NAME_SIZE - 1);
    row.name[COLUMN_NAME_SIZE - 1] = '\0';
    std::strncpy(row.email, email.c_str(), COLUMN_EMAIL_SIZE - 1);
    row.email[COLUMN_EMAIL_SIZE - 1] = '\0';
    return row;
}

void print_row(const Row& row) {
    if (!row.is_deleted) {
        std::string name(row.name);
        std::string email(row.email);
        // Trim trailing spaces and nulls
        name.erase(name.find_last_not_of(" \0") + 1);
        email.erase(email.find_last_not_of(" \0") + 1);
        std::cout << "(" << row.id << ", " << name << ", " << email << ")" << std::endl;
    }
}