#ifndef PAGER_H
#define PAGER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdint>
#include "row.h" // Include row.h to get ROW_SIZE

// Define offsets within Page 0 for the file header
const uint32_t NUM_ROWS_OFFSET = 0;
const uint32_t MIN_FILE_HEADER_SIZE = NUM_ROWS_OFFSET + sizeof(uint32_t);


const uint32_t PAGE_SIZE = 4096;
const uint32_t TABLE_MAX_PAGES = 100;
const uint32_t ROWS_PER_PAGE_PAGE_0 = (PAGE_SIZE - MIN_FILE_HEADER_SIZE) / ROW_SIZE;
const uint32_t ROWS_PER_PAGE_OTHER_PAGES = PAGE_SIZE / ROW_SIZE;

const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE_PAGE_0 + (ROWS_PER_PAGE_OTHER_PAGES * (TABLE_MAX_PAGES - 1));


// Structure representing the pager, which manages pages in memory and on disk
struct Pager {
    FILE* file;
    uint32_t file_length;
    void* pages[TABLE_MAX_PAGES];
    bool pages_dirty[TABLE_MAX_PAGES];
};

Pager* pager_open(const std::string& filename);
void* get_page(Pager* pager, uint32_t page_num);
void pager_flush(Pager* pager, uint32_t page_num, uint32_t size);

#endif
