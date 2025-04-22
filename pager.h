#ifndef PAGER_H
#define PAGER_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdint>
#include "row.h"

using namespace std;

// === Constants ===
const uint32_t PAGE_SIZE = 4096;
const uint32_t TABLE_MAX_PAGES = 100;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

// === Pager Struct ===
struct Pager {
    FILE* file;
    uint32_t file_length;
    void* pages[TABLE_MAX_PAGES];
};

// === Open DB file and init pager ===
Pager* pager_open(const string& filename) {
    FILE* db_file = fopen(filename.c_str(), "r+b");
    if (!db_file) {
        db_file = fopen(filename.c_str(), "w+b");
        if (!db_file) {
            cerr << "Unable to open file\n";
            exit(EXIT_FAILURE);
        }
    }

    fseek(db_file, 0, SEEK_END);
    uint32_t file_length = ftell(db_file);

    Pager* pager = new Pager();
    pager->file = db_file;
    pager->file_length = file_length;

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        pager->pages[i] = nullptr;
    }

    return pager;
}

// === Load page into memory if not loaded ===
void* get_page(Pager* pager, uint32_t page_num) {
    if (page_num >= TABLE_MAX_PAGES) {
        cerr << "Tried to fetch page number out of bounds: " << page_num << "\n";
        exit(EXIT_FAILURE);
    }

    if (pager->pages[page_num] == nullptr) {
        void* page = malloc(PAGE_SIZE);
        uint32_t num_pages = pager->file_length / PAGE_SIZE;

        if (pager->file_length % PAGE_SIZE != 0) {
            num_pages += 1;
        }

        if (page_num < num_pages) {
            fseek(pager->file, page_num * PAGE_SIZE, SEEK_SET);
            fread(page, PAGE_SIZE, 1, pager->file);
        }

        pager->pages[page_num] = page;
    }

    return pager->pages[page_num];
}

// === Write page to disk ===
void pager_flush(Pager* pager, uint32_t page_num, uint32_t size) {
    if (pager->pages[page_num] == nullptr) {
        cerr << "Tried to flush null page\n";
        return;
    }

    fseek(pager->file, page_num * PAGE_SIZE, SEEK_SET);
    fwrite(pager->pages[page_num], size, 1, pager->file);
}

#endif
