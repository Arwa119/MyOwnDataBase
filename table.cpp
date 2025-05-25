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

Table* db_open(const std::string& filename) {
    Pager* pager = pager_open(filename);
    Table* table = new Table();
    table->pager = pager;
    table->num_rows = 0;
    table->btree = new Btree();
    load_table_data(table);
    return table;
}

void db_close(Table* table) {
    Pager* pager = table->pager;
    uint32_t last_allocated_row_page = (table->num_rows > 0) ? (table->num_rows - 1) / ROWS_PER_PAGE_PAGE_0 : 0;

    void* page0 = get_page(pager, 0);
    std::memcpy((char*)page0 + NUM_ROWS_OFFSET, &table->num_rows, sizeof(table->num_rows));
    pager->pages_dirty[0] = true;

    for (uint32_t i = 0; i <= last_allocated_row_page; i++) {
        if (pager->pages[i] != nullptr && pager->pages_dirty[i]) {
            pager_flush(pager, i, PAGE_SIZE);
        }
        if (pager->pages[i] != nullptr) {
            std::free(pager->pages[i]);
            pager->pages[i] = nullptr;
        }
    }

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
    std::cout << "Closed table\n";
}

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
    void* slot = (page_num == 0) ? (char*)page + MIN_FILE_HEADER_SIZE + row_offset_in_page * ROW_SIZE
                                 : (char*)page + row_offset_in_page * ROW_SIZE;
    return slot;
}

void load_table_data(Table* table) {
    void* page0 = get_page(table->pager, 0);
    if (table->pager->file_length >= MIN_FILE_HEADER_SIZE) {
        std::memcpy(&table->num_rows, (char*)page0 + NUM_ROWS_OFFSET, sizeof(table->num_rows));
    } else {
        table->num_rows = 0;
    }

    uint32_t duplicates_skipped = 0;
    uint32_t deleted_rows_skipped = 0;

    for (uint32_t i = 0; i < table->num_rows; ++i) {
        void* row_slot = get_row_slot(table, i);
        Row row;
        deserialize_row(row_slot, row);

        if (row.is_deleted) {
            deleted_rows_skipped++;
            continue;
        }

        uint32_t existing_row_num = table->btree->search(row.id);
        if (existing_row_num == std::numeric_limits<uint32_t>::max()) {
            table->btree->insert(row.id, i);
        } else {
            std::cerr << "Warning: Duplicate ID " << row.id << " at row " << i << ", existing at " << existing_row_num << "\n";
            duplicates_skipped++;
        }
    }
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

bool update_row(Table* table, int id, const std::string& name, const std::string& email) {
    uint32_t row_num = table->btree->search(id);
    if (row_num == std::numeric_limits<uint32_t>::max() || is_row_freed(table, row_num)) {
        return false; // Row not found or deleted
    }

    // Create updated row
    Row row = create_row(id, name, email);
    row.is_deleted = false;

    // Write to disk
    void* row_slot = get_row_slot(table, row_num);
    serialize_row(row, row_slot);

    // Mark page as dirty
    uint32_t page_num;
    if (row_num < ROWS_PER_PAGE_PAGE_0) {
        page_num = 0;
    } else {
        uint32_t row_num_after_page_0 = row_num - ROWS_PER_PAGE_PAGE_0;
        page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
    }
    table->pager->pages_dirty[page_num] = true;

    std::cout << "Updated row: id=" << id << ", row_num=" << row_num << "\n";
    return true;
}