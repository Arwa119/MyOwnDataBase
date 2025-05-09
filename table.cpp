#include "table.h"
#include "pager.h"
#include "row_serialization.h"
#include "btree.h"
#include "row.h"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <vector>
#include <algorithm>
#include <cstring>

// Open or create a database table file
Table* db_open(const std::string& filename) {
    Pager* pager = pager_open(filename);

    Table* table = new Table();
    table->pager = pager;
    table->num_rows = 0;

    table->btree = new Btree();

    load_table_data(table);

    return table;
}

// Close the database table, flushing all dirty pages to disk
void db_close(Table* table) {
    Pager* pager = table->pager;
    uint32_t last_allocated_row_page = (table->num_rows > 0) ? (table->num_rows - 1) / ROWS_PER_PAGE_PAGE_0 : 0;


    // --- Persist File Header (num_rows) ---
    void* page0 = get_page(pager, 0);
    std::memcpy((char*)page0 + NUM_ROWS_OFFSET, &table->num_rows, sizeof(table->num_rows));
    pager->pages_dirty[0] = true;
    // --- End Persist File Header ---


    // Flush all dirty pages up to and including the last page containing data
    for (uint32_t i = 0; i <= last_allocated_row_page; i++) {
        if (pager->pages[i] != nullptr && pager->pages_dirty[i]) {
            pager_flush(pager, i, PAGE_SIZE);
        }
        if (pager->pages[i] != nullptr) {
             std::free(pager->pages[i]);
             pager->pages[i] = nullptr;
        }
    }

    // Free any remaining cached pages beyond the last data page
    for (uint32_t i = last_allocated_row_page + 1; i < TABLE_MAX_PAGES; i++) {
         if (pager->pages[i] != nullptr) {
             std::free(pager->pages[i]);
             pager->pages[i] = nullptr;
         }
    }

    std::fclose(pager->file);
    delete pager;
    delete table->btree;
    delete table;
}

// Get a pointer to the memory slot for a specific row
void* get_row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num;
    uint32_t row_offset_in_page;

    if (row_num < ROWS_PER_PAGE_PAGE_0) {
        page_num = 0;
        row_offset_in_page = row_num;
    } else {
        uint32_t row_num_after_page_0 = row_num - ROWS_PER_PAGE_PAGE_0;
        page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
        row_offset_in_page = row_num_after_page_0 % ROWS_PER_PAGE_OTHER_PAGES;
    }

    void* page = get_page(table->pager, page_num);

    if (page_num == 0) {
        return (char*)page + MIN_FILE_HEADER_SIZE + row_offset_in_page * ROW_SIZE;
    } else {
        return (char*)page + row_offset_in_page * ROW_SIZE;
    }
}

// Load existing data (num_rows and B-tree) from the database file
void load_table_data(Table* table) {
    void* page0 = get_page(table->pager, 0);

    // --- Load num_rows ---
    if (table->pager->file_length >= sizeof(uint32_t)) {
        std::memcpy(&table->num_rows, (char*)page0 + NUM_ROWS_OFFSET, sizeof(table->num_rows));
    } else {
        table->num_rows = 0;
    }
    // --- End Load num_rows ---


    // --- Load B-tree Index ---
    uint32_t duplicates_skipped = 0;
    uint32_t deleted_rows_skipped_for_indexing = 0;

    for (uint32_t i = 0; i < table->num_rows; ++i) {
         uint32_t row_byte_offset;
         if (i < ROWS_PER_PAGE_PAGE_0) {
             row_byte_offset = MIN_FILE_HEADER_SIZE + i * ROW_SIZE;
         } else {
             uint32_t row_num_after_page_0 = i - ROWS_PER_PAGE_PAGE_0;
             uint32_t page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
             uint32_t row_offset_in_page = row_num_after_page_0 % ROWS_PER_PAGE_OTHER_PAGES;
             row_byte_offset = page_num * PAGE_SIZE + row_offset_in_page * ROW_SIZE;
         }

         if (row_byte_offset + ROW_SIZE > table->pager->file_length) {
             std::cerr << "Warning: Attempted to load row " << i << " which is beyond file bounds based on file_length.\n";
             break;
         }

        void* row_slot = get_row_slot(table, i);
        Row row;
        deserialize_row(row_slot, row);

        if (row.is_deleted) {
            deleted_rows_skipped_for_indexing++;
            continue;
        }

        uint32_t existing_row_num = table->btree->search(row.id);

        if (existing_row_num == std::numeric_limits<uint32_t>::max()) {
            table->btree->insert(row.id, i);
        } else {
            std::cerr << "Warning: Duplicate ID " << row.id
                      << " found at row number " << i
                      << ". Skipping indexing for this duplicate.\n";
            duplicates_skipped++;
        }
    }
    // --- End Load B-tree Index ---
}

bool is_row_freed(Table* table, uint32_t row_num) {
    if (row_num >= table->num_rows) {
        return true;
    }

    void* row_slot = get_row_slot(table, row_num);
    Row row;
    deserialize_row(row_slot, row);
    return row.is_deleted;
}
