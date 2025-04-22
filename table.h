// table.h
#ifndef TABLE_H
#define TABLE_H

#include <cstdint>
#include "pager.h"
#include "row_serialization.h"

struct Table {
    uint32_t num_rows;
    Pager* pager;
};

// Allocate and open a table
Table* db_open(const string& filename) {
    Pager* pager = pager_open(filename);
    uint32_t num_rows = pager->file_length / ROW_SIZE;

    Table* table = new Table();
    table->pager = pager;
    table->num_rows = num_rows;
    return table;
}

// Clean up and write all pages to disk
void db_close(Table* table) {
    Pager* pager = table->pager;
    uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;

    for (uint32_t i = 0; i < num_full_pages; i++) {
        if (pager->pages[i] == nullptr) {
            continue;
        }
        pager_flush(pager, i, PAGE_SIZE);
        free(pager->pages[i]);
        pager->pages[i] = nullptr;
    }

    // Handle the last partially-filled page
    uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0) {
        uint32_t page_num = num_full_pages;
        if (pager->pages[page_num] != nullptr) {
            pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
            free(pager->pages[page_num]);
            pager->pages[page_num] = nullptr;
        }
    }

    fclose(pager->file);
    delete pager;
    delete table;
}

// Return a memory slot for writing/reading a specific row
void* get_row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = get_page(table->pager, page_num);

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    return (char*)page + row_offset * ROW_SIZE;
}

#endif
