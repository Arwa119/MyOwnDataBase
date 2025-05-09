#ifndef ROW_H
#define ROW_H

#include <string>
#include <cstring>
#include <iostream>
#include <cstdint>

const uint32_t COLUMN_NAME_SIZE = 32;
const uint32_t COLUMN_EMAIL_SIZE = 255;

struct Row{
    bool is_deleted; // Flag for deletion status
    int id;
    char name[COLUMN_NAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
};

const uint32_t ROW_SIZE = sizeof(bool) + sizeof(int) + COLUMN_NAME_SIZE + COLUMN_EMAIL_SIZE;

void print_row(const Row& row);
Row create_row(int id , const std::string& name,const std::string& email);

#endif
