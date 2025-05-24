#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include <string>
#include <vector>
#include <fstream>
#include "table.h"

enum class TransactionState {
    ACTIVE,
    COMMITTED,
    ROLLED_BACK
};

struct TransactionLogEntry {
    std::string operation; // e.g., "INSERT", "DELETE", "UPDATE"
    uint32_t row_num;     // Row number affected
    Row row_data;         // Row data for undo/redo
    std::string table_name; // Affected table
};

class TransactionManager {
private:
    std::string log_file_path;
    std::ofstream log_file;
    bool in_transaction;
    TransactionState state;
    std::vector<TransactionLogEntry> log_entries;
    std::string current_db;
    Table* locked_table;

public:
    TransactionManager(const std::string& db_name);
    ~TransactionManager();

    bool beginTransaction(Table* table);
    bool commit();
    bool rollback(Table* table);
    bool logInsert(const std::string& table_name, uint32_t row_num, const Row& row);
    bool logDelete(const std::string& table_name, uint32_t row_num, const Row& row);
    bool logUpdate(const std::string& table_name, uint32_t row_num, const Row& old_row);
    bool isInTransaction() const { return in_transaction; }
    bool recover(Table* table);
    void setCurrentDatabase(const std::string& db_name);
};

#endif