#include "transaction_manager.h"
#include "row_serialization.h"
#include <iostream>
#include <fstream>
#include <sstream>

TransactionManager::TransactionManager(const std::string& db_name) {
    setCurrentDatabase(db_name);
}

TransactionManager::~TransactionManager() {
    if (log_file.is_open()) {
        log_file.close();
    }
}

void TransactionManager::setCurrentDatabase(const std::string& db_name) {
    current_db = db_name;
    log_file_path = "./databases/" + db_name + "/txn_log.db";
    if (log_file.is_open()) {
        log_file.close();
    }
    log_file.open(log_file_path, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Error: Could not open transaction log file.\n";
    }
}

bool TransactionManager::beginTransaction(uint32_t transaction_id, Table* table, LockType lock_type) {
    if (transaction_states.find(transaction_id) != transaction_states.end() &&
        transaction_states[transaction_id] == TransactionState::ACTIVE) {
        std::cout << "Error: Transaction " << transaction_id << " already in progress.\n";
        return false;
    }
    
    // Don't acquire locks during begin - only when operations are performed
    transaction_states[transaction_id] = TransactionState::ACTIVE;
    transaction_tables[transaction_id] = table;
    transaction_log_entries[transaction_id].clear(); // Use per-transaction logs
    return true;
}

bool TransactionManager::commit(uint32_t transaction_id) {
    if (transaction_states.find(transaction_id) == transaction_states.end() ||
        transaction_states[transaction_id] != TransactionState::ACTIVE) {
        std::cout << "Error: No active transaction " << transaction_id << " to commit.\n";
        return false;
    }

    // Flush log to disk
    if (log_file.is_open()) {
        log_file.flush();
    }

    // Clear log file
    log_file.close();
    std::ofstream clear_log(log_file_path, std::ios::trunc);
    if (!clear_log.is_open()) {
        std::cerr << "Error: Could not clear transaction log.\n";
        return false;
    }
    clear_log.close();
    log_file.open(log_file_path, std::ios::app);

    transaction_states[transaction_id] = TransactionState::COMMITTED;
    releaseLocks(transaction_id);
    transaction_log_entries.erase(transaction_id);
    return true;
}

bool TransactionManager::rollback(uint32_t transaction_id, Table* table) {
    if (transaction_states.find(transaction_id) == transaction_states.end() ||
        transaction_states[transaction_id] != TransactionState::ACTIVE) {
        return false;
    }

    // Undo operations in reverse order
    auto& entries = transaction_log_entries[transaction_id];
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->operation == "INSERT") {
            Row row;
            deserialize_row(get_row_slot(table, it->row_num), row);
            row.is_deleted = true;
            serialize_row(row, get_row_slot(table, it->row_num));
            table->btree->remove(row.id);
            table->num_rows--;
        } else if (it->operation == "DELETE" || it->operation == "UPDATE") {
            serialize_row(it->row_data, get_row_slot(table, it->row_num));
            if (it->operation == "DELETE") {
                table->btree->insert(it->row_data.id, it->row_num);
            }
        }
        uint32_t page_num = (it->row_num < ROWS_PER_PAGE_PAGE_0) ? 0 : 1 + ((it->row_num - ROWS_PER_PAGE_PAGE_0) / ROWS_PER_PAGE_OTHER_PAGES);
        table->pager->pages_dirty[page_num] = true;
    }

    // Clear log
    log_file.close();
    std::ofstream clear_log(log_file_path, std::ios::trunc);
    if (!clear_log.is_open()) {
        std::cerr << "Error: Could not clear transaction log.\n";
        return false;
    }
    clear_log.close();
    log_file.open(log_file_path, std::ios::app);

    transaction_states[transaction_id] = TransactionState::ROLLED_BACK;
    releaseLocks(transaction_id);
    transaction_log_entries.erase(transaction_id);
    return true;
}

bool TransactionManager::logInsert(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& row) {
    if (!isInTransaction(transaction_id)) {
        return true; // Non-transactional operation
    }

    TransactionLogEntry entry;
    entry.operation = "INSERT";
    entry.table_name = table_name;
    entry.row_num = row_num;
    entry.row_data = row;
    transaction_log_entries[transaction_id].push_back(entry);

    if (log_file.is_open()) {
        log_file << "INSERT|" << table_name << "|" << row_num << "|"
                 << row.id << "," << row.name << "," << row.email << "\n";
        log_file.flush();
    }
    return true;
}

bool TransactionManager::logDelete(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& row) {
    if (!isInTransaction(transaction_id)) {
        return true; // Non-transactional operation
    }

    TransactionLogEntry entry;
    entry.operation = "DELETE";
    entry.table_name = table_name;
    entry.row_num = row_num;
    entry.row_data = row;
    transaction_log_entries[transaction_id].push_back(entry); // <-- FIXED

    if (log_file.is_open()) {
        log_file << "DELETE|" << table_name << "|" << row_num << "|"
                 << row.id << "," << row.name << "," << row.email << "\n";
        log_file.flush();
    }
    return true;
}

bool TransactionManager::logUpdate(uint32_t transaction_id, const std::string& table_name, uint32_t row_num, const Row& old_row) {
    if (!isInTransaction(transaction_id)) {
        return true; // Non-transactional operation
    }

    TransactionLogEntry entry;
    entry.operation = "UPDATE";
    entry.table_name = table_name;
    entry.row_num = row_num;
    entry.row_data = old_row;
    transaction_log_entries[transaction_id].push_back(entry); // <-- FIXED

    if (log_file.is_open()) {
        log_file << "UPDATE|" << table_name << "|" << row_num << "|"
                 << old_row.id << "," << old_row.name << "," << old_row.email << "\n";
        log_file.flush();
    }
    return true;
}

bool TransactionManager::isInTransaction(uint32_t transaction_id) const {
    auto it = transaction_states.find(transaction_id);
    return it != transaction_states.end() && it->second == TransactionState::ACTIVE;
}

bool TransactionManager::recover(Table* table) {
    std::ifstream log_reader(log_file_path);
    if (!log_reader.is_open()) {
        return true; // No log file, nothing to recover
    }

    std::vector<TransactionLogEntry> recovery_entries;
    std::string line;
    while (std::getline(log_reader, line)) {
        std::stringstream ss(line);
        std::string operation, table_name, row_data_str;
        uint32_t row_num;
        std::getline(ss, operation, '|');
        std::getline(ss, table_name, '|');
        ss >> row_num;
        ss.ignore(1); // Skip '|'
        std::getline(ss, row_data_str);

        TransactionLogEntry entry;
        entry.operation = operation;
        entry.table_name = table_name;
        entry.row_num = row_num;

        // Parse row data
        std::stringstream row_ss(row_data_str);
        std::string id_str, name, email;
        std::getline(row_ss, id_str, ',');
        std::getline(row_ss, name, ',');
        std::getline(row_ss, email);
        entry.row_data.id = std::stoi(id_str);
        std::strncpy(entry.row_data.name, name.c_str(), COLUMN_NAME_SIZE - 1);
        entry.row_data.name[COLUMN_NAME_SIZE - 1] = '\0';
        std::strncpy(entry.row_data.email, email.c_str(), COLUMN_EMAIL_SIZE - 1);
        entry.row_data.email[COLUMN_EMAIL_SIZE - 1] = '\0';
        entry.row_data.is_deleted = false;

        recovery_entries.push_back(entry);
    }
    log_reader.close();

    // Roll back uncommitted transactions
    for (auto it = recovery_entries.rbegin(); it != recovery_entries.rend(); ++it) {
        if (it->operation == "INSERT") {
            Row row;
            deserialize_row(get_row_slot(table, it->row_num), row);
            row.is_deleted = true;
            serialize_row(row, get_row_slot(table, it->row_num));
            table->btree->remove(row.id);
            table->num_rows--;
        } else if (it->operation == "DELETE" || it->operation == "UPDATE") {
            serialize_row(it->row_data, get_row_slot(table, it->row_num));
            if (it->operation == "DELETE") {
                table->btree->insert(it->row_data.id, it->row_num);
            }
        }
        uint32_t page_num = (it->row_num < ROWS_PER_PAGE_PAGE_0) ? 0 : 1 + ((it->row_num - ROWS_PER_PAGE_PAGE_0) / ROWS_PER_PAGE_OTHER_PAGES);
        table->pager->pages_dirty[page_num] = true;
    }

    // Clear log file
    std::ofstream clear_log(log_file_path, std::ios::trunc);
    if (!clear_log.is_open()) {
        std::cerr << "Error: Could not clear transaction log during recovery.\n";
        return false;
    }
    clear_log.close();
    log_file.open(log_file_path, std::ios::app);
    return true;
}

bool TransactionManager::acquireLock(uint32_t transaction_id, const std::string& table_name, LockType lock_type) {
    if (lock_manager.detectDeadlock(transaction_id, table_name, lock_type)) {
        std::cout << "Error: Deadlock detected for transaction " << transaction_id << ".\n";
        return false;
    }
    return lock_manager.acquireLock(transaction_id, table_name, lock_type);
}

void TransactionManager::releaseLocks(uint32_t transaction_id) {
    lock_manager.releaseAllLocks(transaction_id);
}

void TransactionManager::rollbackAllActiveTransactions() {
    for (auto& pair : transaction_tables) {
        uint32_t transaction_id = pair.first;
        Table* table = pair.second;
        if (isInTransaction(transaction_id)) {
            rollback(transaction_id, table);
        }
    }
}