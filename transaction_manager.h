#ifndef TRANSACTION_MANAGER_H
#define TRANSACTION_MANAGER_H

#include <string>
#include <vector>
#include <fstream>
#include "table.h"
#include "lock_manager.h"

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
    std::unordered_map<uint32_t, std::vector<TransactionLogEntry>> transaction_log_entries;    std::string current_db;
    LockManager lock_manager;
    std::unordered_map<uint32_t, TransactionState> transaction_states;
    std::unordered_map<uint32_t, Table*> transaction_tables;

public:
    TransactionManager(const std::string& db_name);
    ~TransactionManager();

    bool beginTransaction(uint32_t transaction_id, Table* table, LockType lock_type);
    bool commit(uint32_t transaction_id);
    bool rollback(uint32_t transaction_id, Table* table);
    bool logInsert(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& row);
    bool logDelete(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& row);
    bool logUpdate(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& old_row);
    bool isInTransaction(uint32_t transaction_id) const;
    bool recover(Table* table);
    void setCurrentDatabase(const std::string& db_name);
    bool acquireLock(uint32_t transaction_id, const std::string& table_name, LockType lock_type);
    void releaseLocks(uint32_t transaction_id);
    void rollbackAllActiveTransactions(); // New method to rollback all active transactions
};

#endif