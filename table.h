#ifndef TABLE_H
#define TABLE_H

#include "btree.h"
#include "pager.h"
#include "row.h"
#include <string>
#include <limits>

struct Table {
    Pager* pager;
    uint32_t num_rows;
    Btree* btree;
};

Table* db_open(const std::string& filename);
void db_close(Table* table);
void* get_row_slot(Table* table, uint32_t row_num);
void load_table_data(Table* table);
bool is_row_freed(Table* table, uint32_t row_num);
bool update_row(Table* table, int id, const std::string& name, const std::string& email);

#endif