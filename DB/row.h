#ifndef ROW_H
#define ROW_H

#include <string>
#include <cstring>
#include <iostream>
using namespace std;

const uint32_t COLUMN_NAME_SIZE = 32;
const uint32_t COLUMN_EMAIL_SIZE = 255;

struct Row{
    int id;
    char name[COLUMN_NAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];

};

const uint32_t ROW_SIZE = sizeof(Row);

void print_row(const Row& row){
    cout << "(" << row.id << "," << row.name << "," << row.email << ")"<< endl;
}
Row create_row(int id , const string& name,const string& email)
{
    Row row{};
    row.id = id ;
    strncpy(row.name,name.c_str(),COLUMN_NAME_SIZE);
    strncpy(row.email,email.c_str(),COLUMN_EMAIL_SIZE);
    return row;
}

#endif