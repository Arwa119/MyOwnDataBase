#include <iostream>
#include <string>
#include <sstream>
#include <cstdint>
#include <limits>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include "catalog.h"
#include "input_handler.h"
#include "row.h"
#include "row_serialization.h"
#include "pager.h"
#include "table.h"
#include "btree.h"
#include "transaction_manager.h"
#include "parser.h"

int main() {
    DatabaseCatalog catalog("master");
    TransactionManager txn_manager("master");
    Table* current_table = nullptr;
    std::string current_db = "master";
    std::string current_table_name;

    std::string input;
    while (true) {
        std::cout << current_db << (current_table_name.empty() ? "" : "." + current_table_name) << " > ";
        std::getline(std::cin, input);

        if (input == ".exit") {
            if (current_table) {
                catalog.closeTable(current_table);
            }
            txn_manager.rollbackAllActiveTransactions();
            std::cout << "Exiting...\n";
            break;
        }

        uint32_t transaction_id = 0;
        std::string command_input = input;
        size_t colon_pos = input.find(':');
        if (colon_pos != std::string::npos) {
            std::string tid_str = input.substr(0, colon_pos);
            if (tid_str.length() > 1 && tid_str[0] == 'T' && std::all_of(tid_str.begin() + 1, tid_str.end(), ::isdigit)) {
                transaction_id = std::stoi(tid_str.substr(1));
                command_input = input.substr(colon_pos + 1);
                command_input.erase(0, command_input.find_first_not_of(" \t"));
            }
        }

        if (command_input == ".btree") {
            if (!current_table) {
                std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
            } else {
                std::cout << "Printing B-tree structure:\n";
                current_table->btree->printTree(current_table->btree->getRoot());
            }
            continue;
        }

        CommandType command = parse_command(command_input);
        switch (command) {
            case CommandType::CREATE_DB: {
                std::stringstream ss(command_input);
                std::string create_keyword, db_keyword, db_name;
                ss >> create_keyword >> db_keyword >> db_name;

                if (ss.fail() || create_keyword != "create" || db_keyword != "database") {
                    std::cout << "Syntax error. Usage: create database <database_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after database name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot create database during an active transaction.\n";
                    break;
                }

                if (catalog.createDatabase(db_name)) {
                    std::cout << "Database '" << db_name << "' created successfully.\n";
                } else {
                    std::cout << "Failed to create database '" << db_name << "'.\n";
                }
                break;
            }
            case CommandType::DROP_DB: {
                std::stringstream ss(command_input);
                std::string drop_keyword, db_keyword, db_name;
                ss >> drop_keyword >> db_keyword >> db_name;

                if (ss.fail() || drop_keyword != "drop" || db_keyword != "database") {
                    std::cout << "Syntax error. Usage: drop database <database_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after database name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot drop database during an active transaction.\n";
                    break;
                }

                if (current_table) {
                    catalog.closeTable(current_table);
                    current_table = nullptr;
                    current_table_name.clear();
                }

                txn_manager.setCurrentDatabase("master");

                if (catalog.dropDatabase(db_name)) {
                    if (current_db == db_name) {
                        current_db = "master";
                        current_table = nullptr;
                        current_table_name.clear();
                        txn_manager.setCurrentDatabase("master");
                    }
                    std::cout << "Database '" << db_name << "' dropped successfully.\n";
                } else {
                    std::cout << "Failed to drop database '" << db_name << "'.\n";
                }
                break;
            }
            case CommandType::USE_DB: {
                std::stringstream ss(command_input);
                std::string use_keyword, db_name;
                ss >> use_keyword >> db_name;

                if (ss.fail() || use_keyword != "use") {
                    std::cout << "Syntax error. Usage: use <database_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after database name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot change database during an active transaction.\n";
                    break;
                }

                if (current_table) {
                    catalog.closeTable(current_table);
                    current_table = nullptr;
                    current_table_name.clear();
                }

                if (catalog.useDatabase(db_name)) {
                    current_db = db_name;
                    txn_manager.setCurrentDatabase(db_name);
                    std::cout << "Now using database '" << db_name << "'.\n";
                } else {
                    std::cout << "Error: Database '" << db_name << "' does not exist.\n";
                }
                break;
            }
            case CommandType::USE_TABLE: {
                std::stringstream ss(command_input);
                std::string use_keyword, table_keyword, table_name;
                ss >> use_keyword >> table_keyword >> table_name;

                if (ss.fail() || use_keyword != "use" || table_keyword != "table") {
                    std::cout << "Syntax error. Usage: use table <table_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after table name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot change table during an active transaction.\n";
                    break;
                }

                if (current_table) {
                    catalog.closeTable(current_table);
                    current_table = nullptr;
                    current_table_name.clear();
                }

                Table* table = catalog.openTable(table_name);
                if (table) {
                    current_table = table;
                    current_table_name = table_name;
                    txn_manager.recover(table);
                    std::cout << "Now using table '" << table_name << "' in database '" << current_db << "'.\n";
                } else {
                    std::cout << "Error: Table '" << table_name << "' does not exist in database '" << current_db << "'.\n";
                }
                break;
            }
            case CommandType::CREATE_TABLE: {
                std::stringstream ss(command_input);
                std::string create_keyword, table_keyword, table_name;
                ss >> create_keyword >> table_keyword >> table_name;

                if (ss.fail() || create_keyword != "create" || table_keyword != "table") {
                    std::cout << "Syntax error. Usage: create table <table_name> (<column_name> <type>, ...)\n";
                    break;
                }

                std::string columns_str;
                std::getline(ss, columns_str);

                size_t open_paren = columns_str.find('(');
                size_t close_paren = columns_str.find_last_of(')');

                if (open_paren == std::string::npos || close_paren == std::string::npos) {
                    std::cout << "Syntax error: Missing parentheses in column definitions.\n";
                    break;
                }

                std::string columns_content = columns_str.substr(open_paren + 1, close_paren - open_paren - 1);
                std::stringstream columns_stream(columns_content);
                std::string column_def;

                std::vector<std::string> column_names;
                std::vector<std::string> column_types;

                while (std::getline(columns_stream, column_def, ',')) {
                    column_def.erase(0, column_def.find_first_not_of(" \t"));
                    column_def.erase(column_def.find_last_not_of(" \t") + 1);

                    std::stringstream col_stream(column_def);
                    std::string col_name, col_type;
                    col_stream >> col_name >> col_type;

                    if (col_stream.fail() || col_name.empty() || col_type.empty()) {
                        std::cout << "Syntax error in column definition: " << column_def << "\n";
                        break;
                    }

                    column_names.push_back(col_name);
                    column_types.push_back(col_type);
                }

                if (!column_names.empty() && column_names.size() == column_types.size()) {
                    std::vector<std::string> expected_names = {"id", "name", "email"};
                    std::vector<std::string> expected_types = {"int", "varchar", "varchar"};
                    if (column_names != expected_names || column_types != expected_types) {
                        std::cout << "Error: Table schema must be (id int, name varchar, email varchar).\n";
                        break;
                    }

                    if (catalog.createTable(table_name, column_names, column_types)) {
                        if (current_table) {
                            catalog.closeTable(current_table);
                        }
                        current_table = catalog.openTable(table_name);
                        current_table_name = table_name;
                        std::cout << "Table '" << table_name << "' created and opened successfully.\n";
                    } else {
                        std::cout << "Failed to create table '" << table_name << "'.\n";
                    }
                } else {
                    std::cout << "Error: No valid column definitions provided.\n";
                }
                break;
            }
            case CommandType::SHOW_TABLES: {
                std::stringstream ss(command_input);
                std::string show_keyword, tables_keyword;
                ss >> show_keyword >> tables_keyword;

                if (ss.fail() || show_keyword != "show" || tables_keyword != "tables") {
                    std::cout << "Syntax error. Usage: show tables\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after 'tables'.\n";
                    break;
                }

                std::vector<std::string> tables = catalog.listTables();
                if (tables.empty()) {
                    std::cout << "No tables found in the current database.\n";
                } else {
                    std::cout << "Tables in the current database:\n";
                    for (const auto& table_name : tables) {
                        std::cout << "- " << table_name << "\n";
                    }
                }
                break;
            }
            case CommandType::DROP_TABLE: {
                std::stringstream ss(command_input);
                std::string drop_keyword, table_keyword, table_name;
                ss >> drop_keyword >> table_keyword >> table_name;

                if (ss.fail() || drop_keyword != "drop" || table_keyword != "table") {
                    std::cout << "Syntax error. Usage: drop table <table_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after table name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot drop table during an active transaction.\n";
                    break;
                }

                if (current_table && catalog.getTableName(current_table) == table_name) {
                    catalog.closeTable(current_table);
                    current_table = nullptr;
                    current_table_name.clear();
                }

                if (catalog.dropTable(table_name)) {
                    std::cout << "Table '" << table_name << "' dropped successfully.\n";
                } else {
                    std::cout << "Failed to drop table '" << table_name << "'.\n";
                }
                break;
            }
            case CommandType::ALTER_TABLE: {
                std::stringstream ss(command_input);
                std::string alter_keyword, table_keyword, table_name, rename_keyword, to_keyword, new_table_name;
                ss >> alter_keyword >> table_keyword >> table_name >> rename_keyword >> to_keyword >> new_table_name;

                if (ss.fail() || alter_keyword != "alter" || table_keyword != "table" ||
                    rename_keyword != "rename" || to_keyword != "to") {
                    std::cout << "Syntax error. Usage: alter table <table_name> rename to <new_table_name>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after new table name.\n";
                    break;
                }

                if (txn_manager.isInTransaction(transaction_id)) {
                    std::cout << "Error: Cannot alter table during an active transaction.\n";
                    break;
                }

                if (current_table && catalog.getTableName(current_table) == table_name) {
                    catalog.closeTable(current_table);
                    current_table = nullptr;
                    current_table_name.clear();
                }

                if (catalog.alterTableName(table_name, new_table_name)) {
                    std::cout << "Table renamed from '" << table_name << "' to '" << new_table_name << "' successfully.\n";
                } else {
                    std::cout << "Failed to rename table from '" << table_name << "' to '" << new_table_name << "'.\n";
                }
                break;
            }
            case CommandType::INSERT: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }

                if (transaction_id != 0) {
                    if (!txn_manager.acquireLock(transaction_id, current_table_name, LockType::EXCLUSIVE)) {
                        std::cout << "Transaction " << transaction_id << " waiting for exclusive lock on table '" << current_table_name << "'.\n";
                        break;
                    }
                }

                std::stringstream ss(command_input);
                std::string keyword;
                int id;
                std::string name, email;
                ss >> keyword >> id >> name >> email;

                if (ss.fail()) {
                    std::cout << "Syntax error. Usage: insert <id> <name> <email>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after email.\n";
                    break;
                }

                if (name.length() >= COLUMN_NAME_SIZE) {
                    std::cout << "Error: Name is too long (max " << COLUMN_NAME_SIZE - 1 << " characters).\n";
                    break;
                }
                if (email.length() >= COLUMN_EMAIL_SIZE) {
                    std::cout << "Error: Email is too long (max " << COLUMN_EMAIL_SIZE - 1 << " characters).\n";
                    break;
                }

                uint32_t existing_row_num = current_table->btree->search(id);
                if (existing_row_num != std::numeric_limits<uint32_t>::max()) {
                    if (!is_row_freed(current_table, existing_row_num)) {
                        std::cout << "Error: Duplicate ID. Row with ID " << id << " already exists.\n";
                        break;
                    } else {
                        std::cout << "Warning: Duplicate ID " << id << " found but marked as deleted. Inserting new row.\n";
                    }
                }

                uint32_t row_to_insert_at = current_table->num_rows;
                if (row_to_insert_at >= TABLE_MAX_ROWS) {
                    std::cout << "Error: Table full.\n";
                    break;
                }

                Row row = create_row(id, name, email);
                if (txn_manager.isInTransaction(transaction_id)) {
                    txn_manager.logInsert(transaction_id, current_table_name, row_to_insert_at, row);
                }
                void* row_slot = get_row_slot(current_table, row_to_insert_at);
                serialize_row(row, row_slot);
                current_table->num_rows++;
                current_table->btree->insert(id, row_to_insert_at);

                uint32_t page_num;
                if (row_to_insert_at < ROWS_PER_PAGE_PAGE_0) {
                    page_num = 0;
                } else {
                    uint32_t row_num_after_page_0 = row_to_insert_at - ROWS_PER_PAGE_PAGE_0;
                    page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
                }
                current_table->pager->pages_dirty[page_num] = true;
                std::cout << "Row inserted.\n";
                break;
            }
            case CommandType::SELECT: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }

                bool lock_acquired = true;
                if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, current_table_name, LockType::SHARED)) {
                    std::cout << "Transaction " << transaction_id << " waiting for shared lock on table '" << current_table_name << "'.\n";
                    lock_acquired = false;
                }

                if (!lock_acquired) {
                    break;
                }

                std::stringstream ss(command_input);
                std::string keyword, next_word;
                ss >> keyword >> next_word;

                if (ss.fail()) {
                    if (current_table->num_rows == 0) {
                        std::cout << "Table is empty.\n";
                    } else {
                        std::cout << "Retrieving all rows:\n";
                        uint32_t rows_displayed = 0;
                        for (uint32_t i = 0; i < current_table->num_rows; i++) {
                            if (!is_row_freed(current_table, i)) {
                                void* row_slot = get_row_slot(current_table, i);
                                Row row;
                                deserialize_row(row_slot, row);
                                std::cout << "(" << row.id << ", " << row.name << ", " << row.email << ")\n";
                                rows_displayed++;
                            }
                        }
                        if (rows_displayed == 0) {
                            std::cout << "All rows are deleted.\n";
                        }
                    }
                } else if (next_word == "where") {
                    std::string id_keyword, equals_sign;
                    int search_id;
                    ss >> id_keyword >> equals_sign >> search_id;

                    if (ss.fail() || id_keyword != "id" || equals_sign != "=") {
                        std::cout << "Syntax error. Usage: select where id = <id>\n";
                        break;
                    }

                    std::string remaining;
                    if (ss >> remaining) {
                        std::cout << "Syntax error: Extra input after ID.\n";
                        break;
                    }

                    uint32_t found_row_num = current_table->btree->search(search_id);
                    if (found_row_num != std::numeric_limits<uint32_t>::max()) {
                        if (!is_row_freed(current_table, found_row_num)) {
                            std::cout << "Found row with ID " << search_id << ":\n";
                            void* row_slot = get_row_slot(current_table, found_row_num);
                            Row row;
                            deserialize_row(row_slot, row);
                            std::cout << "(" << row.id << ", " << row.name << ", " << row.email << ")\n";
                        } else {
                            std::cout << "Row with ID " << search_id << " is deleted.\n";
                        }
                    } else {
                        std::cout << "Row with ID " << search_id << " not found.\n";
                    }
                } else {
                    std::cout << "Syntax error. Usage: select or select where id = <id>\n";
                }
                break;
            }
            case CommandType::DELETE: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }

                bool lock_acquired = true;
                if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, current_table_name, LockType::EXCLUSIVE)) {
                    std::cout << "Transaction " << transaction_id << " waiting for exclusive lock on table '" << current_table_name << "'.\n";
                    lock_acquired = false;
                }

                if (!lock_acquired) {
                    break;
                }

                std::stringstream ss(command_input);
                std::string keyword, from_keyword, table_name, where_keyword, id_keyword, equals_sign;
                int delete_id;
                ss >> keyword >> from_keyword >> table_name >> where_keyword >> id_keyword >> equals_sign >> delete_id;

                if (ss.fail() || keyword != "delete" || from_keyword != "from" || table_name != "table" ||
                    where_keyword != "where" || id_keyword != "id" || equals_sign != "=") {
                    std::cout << "Syntax error. Usage: delete from table where id = <id>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after ID.\n";
                    break;
                }

                uint32_t row_num_to_delete = current_table->btree->search(delete_id);
                if (row_num_to_delete != std::numeric_limits<uint32_t>::max()) {
                    bool removed_from_btree = current_table->btree->remove(delete_id);
                    if (removed_from_btree) {
                        void* row_slot = get_row_slot(current_table, row_num_to_delete);
                        Row row;
                        deserialize_row(row_slot, row);
                        if (txn_manager.isInTransaction(transaction_id)) {
                            txn_manager.logDelete(transaction_id, current_table_name, row_num_to_delete, row);
                        }
                        row.is_deleted = true;
                        serialize_row(row, row_slot);

                        uint32_t page_num;
                        if (row_num_to_delete < ROWS_PER_PAGE_PAGE_0) {
                            page_num = 0;
                        } else {
                            uint32_t row_num_after_page_0 = row_num_to_delete - ROWS_PER_PAGE_PAGE_0;
                            page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
                        }
                        current_table->pager->pages_dirty[page_num] = true;
                        std::cout << "Row with ID " << delete_id << " deleted.\n";
                    } else {
                        std::cerr << "Error: Key found but failed to remove from B-tree.\n";
                    }
                } else {
                    std::cout << "Row with ID " << delete_id << " not found.\n";
                }
                break;
            }
            case CommandType::UPDATE: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }

                bool lock_acquired = true;
                if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, current_table_name, LockType::EXCLUSIVE)) {
                    std::cout << "Transaction " << transaction_id << " waiting for exclusive lock on table '" << current_table_name << "'.\n";
                    lock_acquired = false;
                }

                if (!lock_acquired) {
                    break;
                }

                std::stringstream ss(command_input);
                std::string keyword, table_keyword, set_keyword, name_keyword, name_equals, name_value,
                            email_keyword, email_equals, email_value, where_keyword, id_keyword, id_equals;
                int update_id;

                ss >> keyword >> table_keyword >> set_keyword >> name_keyword >> name_equals >> name_value >>
                   email_keyword >> email_equals >> email_value >> where_keyword >> id_keyword >> id_equals >> update_id;

                if (ss.fail() || keyword != "update" || table_keyword != "table" || set_keyword != "set" ||
                    name_keyword != "name" || name_equals != "=" || email_keyword != "email" || email_equals != "=" ||
                    where_keyword != "where" || id_keyword != "id" || id_equals != "=") {
                    std::cout << "Syntax error. Usage: update table set name = <name>, email = <email> where id = <id>\n";
                    break;
                }

                std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after ID.\n";
                    break;
                }

                if (name_value.length() >= COLUMN_NAME_SIZE) {
                    std::cout << "Error: Name is too long (max " << COLUMN_NAME_SIZE - 1 << " characters).\n";
                    break;
                }
                if (email_value.length() >= COLUMN_EMAIL_SIZE) {
                    std::cout << "Error: Email is too long (max " << COLUMN_EMAIL_SIZE - 1 << " characters).\n";
                    break;
                }

                uint32_t row_num = current_table->btree->search(update_id);
                if (row_num != std::numeric_limits<uint32_t>::max() && !is_row_freed(current_table, row_num)) {
                    Row old_row;
                    deserialize_row(get_row_slot(current_table, row_num), old_row);
                    if (txn_manager.isInTransaction(transaction_id)) {
                        txn_manager.logUpdate(transaction_id, current_table_name, row_num, old_row);
                    }
                    if (update_row(current_table, update_id, name_value, email_value)) {
                        std::cout << "Row with ID " << update_id << " updated.\n";
                    } else {
                        std::cout << "Row with ID " << update_id << " not found or deleted.\n";
                    }
                } else {
                    std::cout << "Row with ID " << update_id << " not found or deleted.\n";
                }
                break;
            }
            case CommandType::BTREE_CMD: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }
                std::cout << "Printing B-tree structure:\n";
                current_table->btree->printTree(current_table->btree->getRoot());
                break;
            }
            case CommandType::BEGIN_TRANSACTION: {
                if (!current_table) {
                    std::cout << "Error: No table selected. Use 'create table' or 'use table'.\n";
                    break;
                }
                LockType lock_type = (transaction_id == 2) ? LockType::SHARED : LockType::EXCLUSIVE;
                if (!txn_manager.beginTransaction(transaction_id, current_table, lock_type)) {
                    std::cout << "Transaction " << transaction_id << " waiting for "
                              << (lock_type == LockType::SHARED ? "shared" : "exclusive")
                              << " lock on table '" << current_table_name << "'.\n";
                    break;
                }
                std::cout << "Transaction " << transaction_id << " started.\n";
                break;
            }
            case CommandType::COMMIT: {
                if (txn_manager.commit(transaction_id)) {
                    std::cout << "Transaction " << transaction_id << " committed successfully.\n";
                } else {
                    std::cout << "Error: No active transaction " << transaction_id << " to commit.\n";
                }
                break;
            }
            case CommandType::ROLLBACK: {
                if (!current_table) {
                    std::cout << "Error: No table selected for rollback.\n";
                    break;
                }
                if (txn_manager.rollback(transaction_id, current_table)) {
                    std::cout << "Transaction " << transaction_id << " rolled back successfully.\n";
                } else {
                    std::cout << "Error: No active transaction " << transaction_id << " to rollback.\n";
                }
                break;
            }
            default:
                std::cout << "Unrecognized command: " << command_input << "\n";
                break;
        }
    }
    return 0;
}