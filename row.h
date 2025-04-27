#ifndef ROW_H
#define ROW_H
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>

using namespace std;

const uint32_t COLUMN_NAME_SIZE = 32;
const uint32_t COLUMN_EMAIL_SIZE = 255;

struct Row {
    int id;
    char name[COLUMN_NAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];

    // Serialize function
    string serialize() const {
        stringstream ss;
        ss << id << "," << name << "," << email; // Comma-separated values
        return ss.str();
    }
};

const uint32_t ROW_SIZE = sizeof(Row);

// Declarations
void print_row(const Row& row); 
Row create_row(int id, const string& name, const string& email); 

#endif

//here in this file i have updated it in the way that only declerations are in this file and theri procedure in row.cpp becuase it was cauing error