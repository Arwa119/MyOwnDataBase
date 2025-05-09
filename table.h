// table.h
#ifndef TABLE_H
#define TABLE_H

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

struct Pager;

#include "pager.h" 
#include "row_serialization.h"
#include "btree.h"

// Structure representing the database table
struct Table {
    uint32_t num_rows; // Total number of allocated row slots in the file (includes deleted)
    Pager* pager;
    Btree* btree;
};
// Allocate and open a table
Table* db_open(const std::string& filename);
// Clean up and write all pages to disk
void db_close(Table* table);
// Return a memory slot for writing/reading a specific row
void* get_row_slot(Table* table, uint32_t row_num);
// Helper function to load existing data (num_rows and B-tree, checking is_deleted)
void load_table_data(Table* table);
// Helper function to check if a row is marked as freed (deleted)
bool is_row_freed(Table* table, uint32_t row_num);
#endif
