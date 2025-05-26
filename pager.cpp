#include "pager.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

Pager* pager_open(const std::string& filename) {
    FILE* db_file = fopen(filename.c_str(), "r+b");
    bool new_file = false;

    if (!db_file) {
        db_file = fopen(filename.c_str(), "w+b");
        if (!db_file) {
            std::cerr << "Unable to open or create file: " << filename << "\n";
            exit(EXIT_FAILURE);
        }
        new_file = true;
    }

    fseek(db_file, 0, SEEK_END);
    uint32_t file_length = ftell(db_file);
    std::cout << "Opening file: " << filename << ", length: " << file_length << " bytes\n";

    Pager* pager = new Pager();
    pager->file = db_file;
    pager->file_length = file_length;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = nullptr;
        pager->pages_dirty[i] = false;
    }

    if (new_file || file_length < MIN_FILE_HEADER_SIZE) {
        void* page0 = std::malloc(PAGE_SIZE);
        if (page0 == nullptr) {
            std::cerr << "Error allocating memory for page 0\n";
            exit(EXIT_FAILURE);
        }
        std::memset(page0, 0, PAGE_SIZE);
        uint32_t num_rows = 0;
        std::memcpy((char*)page0 + NUM_ROWS_OFFSET, &num_rows, sizeof(num_rows));

        pager->pages[0] = page0;
        pager->pages_dirty[0] = true;
        pager_flush(pager, 0, PAGE_SIZE); // Ensure page 0 is written
        pager->file_length = PAGE_SIZE;
        //std::cout << "Initialized new file with page 0\n";
    }

    return pager;
}

void* get_page(Pager* pager, uint32_t page_num) {
    if (page_num >= TABLE_MAX_PAGES) {
        std::cerr << "Tried to fetch page number out of bounds: " << page_num << "\n";
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == nullptr) {
        void* page = std::malloc(PAGE_SIZE);
        if (page == nullptr) {
            std::cerr << "Error allocating memory for page\n";
            exit(EXIT_FAILURE);
        }
        std::memset(page, 0, PAGE_SIZE);

        uint32_t page_offset_in_file = page_num * PAGE_SIZE;
        if (page_offset_in_file < pager->file_length) {
            fseek(pager->file, page_offset_in_file, SEEK_SET);
            size_t bytes_read = fread(page, 1, PAGE_SIZE, pager->file);
            if (bytes_read < PAGE_SIZE && !feof(pager->file)) {
                std::cerr << "Error reading page " << page_num << "\n";
                exit(EXIT_FAILURE);
            }
        } else {
            std::cout << "Initialized new page " << page_num << "\n";
        }

        pager->pages[page_num] = page;
        pager->pages_dirty[page_num] = false;
    }

    return pager->pages[page_num];
}

void pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {
    if (pager->pages[page_num] == nullptr) {
        std::cerr << "Tried to flush a null page\n";
        return;
    }
    if (page_num >= TABLE_MAX_PAGES) {
        std::cerr << "Tried to flush page number out of bounds: " << page_num << "\n";
        exit(EXIT_FAILURE);
    }
    if (!pager->pages_dirty[page_num]) {
        return;
    }

    fseek(pager->file, page_num * PAGE_SIZE, SEEK_SET);
    size_t bytes_written = fwrite(pager->pages[page_num], 1, size, pager->file);
    if (bytes_written != size) {
        std::cerr << "Error writing page " << page_num << "\n";
        exit(EXIT_FAILURE);
    }
    fflush(pager->file);
    pager->pages_dirty[page_num] = false;
    //std::cout << "Flushed page " << page_num << ", bytes written: " << bytes_written << "\n";
}