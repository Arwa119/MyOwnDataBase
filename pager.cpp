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

        pager->pages[0] = page0;
        pager->pages_dirty[0] = true;

        if (file_length < PAGE_SIZE) {
             fseek(db_file, PAGE_SIZE - 1, SEEK_SET);
             fputc('\0', db_file);
             fflush(db_file);
             pager->file_length = PAGE_SIZE;
        }
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

        uint32_t page_offset_in_file = page_num * PAGE_SIZE;

        if (page_offset_in_file < pager->file_length) {
            uint32_t bytes_to_read = PAGE_SIZE;
            if (page_offset_in_file + bytes_to_read > pager->file_length) {
                bytes_to_read = pager->file_length - page_offset_in_file;
            }

            fseek(pager->file, page_offset_in_file, SEEK_SET);
            size_t bytes_read = fread(page, 1, bytes_to_read, pager->file);

            if (bytes_read < PAGE_SIZE) {
                std::memset((char*)page + bytes_read, 0, PAGE_SIZE - bytes_read);
            }

            if (bytes_read != bytes_to_read && !feof(pager->file)) {
                 std::cerr << "Error reading file\n";
                 exit(EXIT_FAILURE);
            }

        } else {
            std::memset(page, 0, PAGE_SIZE);
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
         std::cerr << "Error writing to file\n";
         exit(EXIT_FAILURE);
    }
    pager->pages_dirty[page_num] = false;
}
