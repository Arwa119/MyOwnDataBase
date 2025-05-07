#ifndef TABLE_H
#define TABLE_H

#include <cstdint>
#include "pager.h"
#include "row_serialization.h"
#include "btree.h" // Include B-Tree

struct Table {
    BTree* btree; // Replace num_rows with a B-Tree instance
};

// Allocate and open a table
Table* db_open(const string& filename) {
    Table* table = new Table();
    table->btree = new BTree(); // Initialize B-Tree
    return table;
}

// Clean up the table
void db_close(Table* table) {
    delete table->btree;
    delete table;
}

#endif
