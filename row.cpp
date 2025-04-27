#include "row.h"
#include <iostream>
#include <cstring>

using namespace std;

// it prints the rows
void print_row(const Row& row) {
    cout << "(" << row.id << ", " << row.name << ", " << row.email << ")" << endl;
}

// creates rows
Row create_row(int id, const string& name, const string& email) {
    Row row{};
    row.id = id;
    strncpy(row.name, name.c_str(), COLUMN_NAME_SIZE);
    strncpy(row.email, email.c_str(), COLUMN_EMAIL_SIZE);
    return row;
}