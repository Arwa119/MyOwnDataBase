#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include "http/cpp-httplib/httplib.h"
#include "json.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <map>
#include <mutex>
#include "catalog.h"
#include "input_handler.h"
#include "row.h"
#include "row_serialization.h"
#include "pager.h"
#include "table.h"
#include "btree.h"
#include "transaction_manager.h"
#include "parser.h"

// Session storage for per-client state
struct Session {
    Table* current_table = nullptr;
    std::string current_db = "master";
    std::string current_table_name;
};
std::map<std::string, Session> sessions; // Map session ID to state
std::mutex sessions_mutex; // Protect session map

void handleCommand(const std::string& command_input, DatabaseCatalog& catalog, TransactionManager& txn_manager,
                  Session& session, uint32_t transaction_id, nlohmann::json& response) {
    std::stringstream response_stream;
    std::stringstream ss(command_input);
    CommandType command = parse_command(command_input);

    switch (command) {
        case CommandType::CREATE_DB: {
            std::string create_keyword, db_keyword, db_name;
            ss >> create_keyword >> db_keyword >> db_name;
            if (ss.fail() || create_keyword != "create" || db_keyword != "database") {
                response["error"] = "Syntax error. Usage: create database <database_name>";
                return;
            }
            if (txn_manager.isInTransaction(transaction_id)) {
                response["error"] = "Cannot create database during an active transaction.";
                return;
            }
            if (catalog.createDatabase(db_name)) {
                response["message"] = "Database '" + db_name + "' created successfully.";
            } else {
                response["error"] = "Failed to create database '" + db_name + "'.";
            }
            break;
        }
        case CommandType::DROP_DB: {
            std::string drop_keyword, db_keyword, db_name;
            ss >> drop_keyword >> db_keyword >> db_name;
            if (ss.fail() || drop_keyword != "drop" || db_keyword != "database") {
                response["error"] = "Syntax error. Usage: drop database <database_name>";
                return;
            }
            if (txn_manager.isInTransaction(transaction_id)) {
                response["error"] = "Cannot drop database during an active transaction.";
                return;
            }
            if (session.current_table) {
                catalog.closeTable(session.current_table);
                session.current_table = nullptr;
                session.current_table_name.clear();
            }
            txn_manager.setCurrentDatabase("master");
            if (catalog.dropDatabase(db_name)) {
                if (session.current_db == db_name) {
                    session.current_db = "master";
                    session.current_table = nullptr;
                    session.current_table_name.clear();
                    txn_manager.setCurrentDatabase("master");
                }
                response["message"] = "Database '" + db_name + "' dropped successfully.";
            } else {
                response["error"] = "Failed to drop database '" + db_name + "'.";
            }
            break;
        }
        case CommandType::USE_DB: {
            std::string use_keyword, db_name;
            ss >> use_keyword >> db_name;
            if (ss.fail() || use_keyword != "use") {
                response["error"] = "Syntax error. Usage: use <database_name>";
                return;
            }
            if (txn_manager.isInTransaction(transaction_id)) {
                response["error"] = "Cannot change database during an active transaction.";
                return;
            }
            if (session.current_table) {
                catalog.closeTable(session.current_table);
                session.current_table = nullptr;
                session.current_table_name.clear();
            }
            if (catalog.useDatabase(db_name)) {
                session.current_db = db_name;
                txn_manager.setCurrentDatabase(db_name);
                response["message"] = "Now using database '" + db_name + "'.";
            } else {
                response["error"] = "Database '" + db_name + "' does not exist.";
            }
            break;
        }
        case CommandType::CREATE_TABLE: {
            std::string create_keyword, table_keyword, table_name;
            ss >> create_keyword >> table_keyword >> table_name;
            if (ss.fail() || create_keyword != "create" || table_keyword != "table") {
                response["error"] = "Syntax error. Usage: create table <table_name> (<column_name> <type>, ...)";
                return;
            }
            std::string columns_str;
            std::getline(ss, columns_str);
            size_t open_paren = columns_str.find('(');
            size_t close_paren = columns_str.find_last_of(')');
            if (open_paren == std::string::npos || close_paren == std::string::npos) {
                response["error"] = "Syntax error: Missing parentheses in column definitions.";
                return;
            }
            std::string columns_content = columns_str.substr(open_paren + 1, close_paren - open_paren - 1);
            std::stringstream columns_stream(columns_content);
            std::string column_def;
            std::vector<std::string> column_names, column_types;
            while (std::getline(columns_stream, column_def, ',')) {
                column_def.erase(0, column_def.find_first_not_of(" \t"));
                column_def.erase(column_def.find_last_not_of(" \t") + 1);
                std::stringstream col_stream(column_def);
                std::string col_name, col_type;
                col_stream >> col_name >> col_type;
                if (col_stream.fail() || col_name.empty() || col_type.empty()) {
                    response["error"] = "Syntax error in column definition: " + column_def;
                    return;
                }
                column_names.push_back(col_name);
                column_types.push_back(col_type);
            }
            if (!column_names.empty() && column_names.size() == column_types.size()) {
                std::vector<std::string> expected_names = {"id", "name", "email"};
                std::vector<std::string> expected_types = {"int", "varchar", "varchar"};
                if (column_names != expected_names || column_types != expected_types) {
                    response["error"] = "Table schema must be (id int, name varchar, email varchar).";
                    return;
                }
                if (catalog.createTable(table_name, column_names, column_types)) {
                    if (session.current_table) {
                        catalog.closeTable(session.current_table);
                    }
                    session.current_table = catalog.openTable(table_name);
                    session.current_table_name = table_name;
                    response["message"] = "Table '" + table_name + "' created and opened successfully.";
                } else {
                    response["error"] = "Failed to create table '" + table_name + "'.";
                }
            } else {
                response["error"] = "No valid column definitions provided.";
            }
            break;
        }
        case CommandType::SHOW_TABLES: {
            std::string show_keyword, tables_keyword;
            ss >> show_keyword >> tables_keyword;
            if (ss.fail() || show_keyword != "show" || tables_keyword != "tables") {
                response["error"] = "Syntax error. Usage: show tables";
                return;
            }
            std::vector<std::string> tables = catalog.listTables();
            if (tables.empty()) {
                response["message"] = "No tables found in the current database.";
            } else {
                nlohmann::json tables_array = nlohmann::json::array();
                for (const auto& table_name : tables) {
                    tables_array.push_back(table_name);
                }
                response["message"] = "Tables in the current database:";
                response["tables"] = tables_array;
            }
            break;
        }
        case CommandType::DROP_TABLE: {
            std::string drop_keyword, table_keyword, table_name;
            ss >> drop_keyword >> table_keyword >> table_name;
            if (ss.fail() || drop_keyword != "drop" || table_keyword != "table") {
                response["error"] = "Syntax error. Usage: drop table <table_name>";
                return;
            }
            if (txn_manager.isInTransaction(transaction_id)) {
                response["error"] = "Cannot drop table during an active transaction.";
                return;
            }
            if (session.current_table && catalog.getTableName(session.current_table) == table_name) {
                catalog.closeTable(session.current_table);
                session.current_table = nullptr;
                session.current_table_name.clear();
            }
            if (catalog.dropTable(table_name)) {
                response["message"] = "Table '" + table_name + "' dropped successfully.";
            } else {
                response["error"] = "Failed to drop table '" + table_name + "'.";
            }
            break;
        }
        case CommandType::ALTER_TABLE: {
            std::string alter_keyword, table_keyword, table_name, rename_keyword, to_keyword, new_table_name;
            ss >> alter_keyword >> table_keyword >> table_name >> rename_keyword >> to_keyword >> new_table_name;
            if (ss.fail() || alter_keyword != "alter" || table_keyword != "table" ||
                rename_keyword != "rename" || to_keyword != "to") {
                response["error"] = "Syntax error. Usage: alter table <table_name> rename to <new_table_name>";
                return;
            }
            if (txn_manager.isInTransaction(transaction_id)) {
                response["error"] = "Cannot alter table during an active transaction.";
                return;
            }
            if (session.current_table && catalog.getTableName(session.current_table) == table_name) {
                catalog.closeTable(session.current_table);
                session.current_table = nullptr;
                session.current_table_name.clear();
            }
            if (catalog.alterTableName(table_name, new_table_name)) {
                response["message"] = "Table renamed from '" + table_name + "' to '" + new_table_name + "' successfully.";
            } else {
                response["error"] = "Failed to rename table from '" + table_name + "' to '" + new_table_name + "'.";
            }
            break;
        }
        case CommandType::INSERT: {
            if (!session.current_table) {
                response["error"] = "No table selected. Use 'use table'.";
                return;
            }
            if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, session.current_table_name, LockType::EXCLUSIVE)) {
                response["error"] = "Transaction " + std::to_string(transaction_id) + " waiting for exclusive lock on table '" + session.current_table_name + "'.";
                return;
            }
            std::string keyword;
            int id;
            std::string name, email;
            ss >> keyword >> id >> name >> email;
            if (ss.fail()) {
                response["error"] = "Syntax error. Usage: insert <id> <name> <email>";
                return;
            }
            if (name.length() >= COLUMN_NAME_SIZE || email.length() >= COLUMN_EMAIL_SIZE) {
                response["error"] = "Name or email too long.";
                return;
            }
            uint32_t existing_row_num = session.current_table->btree->search(id);
            if (existing_row_num != std::numeric_limits<uint32_t>::max() && !is_row_freed(session.current_table, existing_row_num)) {
                response["error"] = "Duplicate ID " + std::to_string(id) + ".";
                return;
            }
            uint32_t row_to_insert_at = session.current_table->num_rows;
            if (row_to_insert_at >= TABLE_MAX_ROWS) {
                response["error"] = "Table full.";
                return;
            }
            Row row = create_row(id, name, email);
            if (txn_manager.isInTransaction(transaction_id)) {
                txn_manager.logInsert(transaction_id, session.current_table_name, row_to_insert_at, row);
            }
            void* row_slot = get_row_slot(session.current_table, row_to_insert_at);
            serialize_row(row, row_slot);
            session.current_table->num_rows++;
            session.current_table->btree->insert(id, row_to_insert_at);
            uint32_t page_num = (row_to_insert_at < ROWS_PER_PAGE_PAGE_0) ? 0 : 1 + ((row_to_insert_at - ROWS_PER_PAGE_PAGE_0) / ROWS_PER_PAGE_OTHER_PAGES);
            session.current_table->pager->pages_dirty[page_num] = true;
            response["message"] = "Row inserted.";
            break;
        }
        case CommandType::SELECT: {
            if (!session.current_table) {
                response["error"] = "No table selected.";
                return;
            }
            if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, session.current_table_name, LockType::SHARED)) {
                response["error"] = "Transaction " + std::to_string(transaction_id) + " waiting for shared lock.";
                return;
            }
            std::string keyword, next_word;
            ss >> keyword >> next_word;
            if (ss.fail()) {
                if (session.current_table->num_rows == 0) {
                    response["message"] = "Table is empty.";
                } else {
                    nlohmann::json rows = nlohmann::json::array();
                    for (uint32_t i = 0; i < session.current_table->num_rows; i++) {
                        if (!is_row_freed(session.current_table, i)) {
                            void* row_slot = get_row_slot(session.current_table, i);
                            Row row;
                            deserialize_row(row_slot, row);
                            rows.push_back({{"id", row.id}, {"name", row.name}, {"email", row.email}});
                        }
                    }
                    response["message"] = "Rows retrieved.";
                    response["rows"] = rows;
                }
            } else if (next_word == "where") {
                std::string id_keyword, equals_sign;
                int search_id;
                ss >> id_keyword >> equals_sign >> search_id;
                if (ss.fail() || id_keyword != "id" || equals_sign != "=") {
                    response["error"] = "Syntax error. Usage: select where id = <id>";
                    return;
                }
                uint32_t found_row_num = session.current_table->btree->search(search_id);
                if (found_row_num != std::numeric_limits<uint32_t>::max() && !is_row_freed(session.current_table, found_row_num)) {
                    void* row_slot = get_row_slot(session.current_table, found_row_num);
                    Row row;
                    deserialize_row(row_slot, row);
                    response["message"] = "Found row.";
                    response["row"] = {{"id", row.id}, {"name", row.name}, {"email", row.email}};
                } else {
                    response["message"] = "Row with ID " + std::to_string(search_id) + " not found or deleted.";
                }
            } else {
                response["error"] = "Syntax error. Usage: select or select where id = <id>";
            }
            break;
        }
        case CommandType::DELETE: {
            if (!session.current_table) {
                response["error"] = "No table selected.";
                return;
            }
            if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, session.current_table_name, LockType::EXCLUSIVE)) {
                response["error"] = "Transaction " + std::to_string(transaction_id) + " waiting for exclusive lock.";
                return;
            }
            std::string keyword, from_keyword, table_name, where_keyword, id_keyword, equals_sign;
            int delete_id;
            ss >> keyword >> from_keyword >> table_name >> where_keyword >> id_keyword >> equals_sign >> delete_id;
            if (ss.fail() || keyword != "delete" || from_keyword != "from" || table_name != "table" ||
                where_keyword != "where" || id_keyword != "id" || equals_sign != "=") {
                response["error"] = "Syntax error. Usage: delete from table where id = <id>";
                return;
            }
            uint32_t row_num_to_delete = session.current_table->btree->search(delete_id);
            if (row_num_to_delete != std::numeric_limits<uint32_t>::max()) {
                bool removed_from_btree = session.current_table->btree->remove(delete_id);
                if (removed_from_btree) {
                    void* row_slot = get_row_slot(session.current_table, row_num_to_delete);
                    Row row;
                    deserialize_row(row_slot, row);
                    if (txn_manager.isInTransaction(transaction_id)) {
                        txn_manager.logDelete(transaction_id, session.current_table_name, row_num_to_delete, row);
                    }
                    row.is_deleted = true;
                    serialize_row(row, row_slot);
                    uint32_t page_num = (row_num_to_delete < ROWS_PER_PAGE_PAGE_0) ? 0 : 1 + ((row_num_to_delete - ROWS_PER_PAGE_PAGE_0) / ROWS_PER_PAGE_OTHER_PAGES);
                    session.current_table->pager->pages_dirty[page_num] = true;
                    response["message"] = "Row with ID " + std::to_string(delete_id) + " deleted.";
                } else {
                    response["error"] = "Key found but failed to remove from B-tree.";
                }
            } else {
                response["message"] = "Row with ID " + std::to_string(delete_id) + " not found.";
            }
            break;
        }
        case CommandType::UPDATE: {
            if (!session.current_table) {
                response["error"] = "No table selected.";
                return;
            }
            if (transaction_id != 0 && !txn_manager.acquireLock(transaction_id, session.current_table_name, LockType::EXCLUSIVE)) {
                response["error"] = "Transaction " + std::to_string(transaction_id) + " waiting for exclusive lock.";
                return;
            }
            std::string keyword, table_keyword, set_keyword, name_keyword, name_equals, name_value,
                        email_keyword, email_equals, email_value, where_keyword, id_keyword, id_equals;
            int update_id;
            ss >> keyword >> table_keyword >> set_keyword >> name_keyword >> name_equals >> name_value >>
               email_keyword >> email_equals >> email_value >> where_keyword >> id_keyword >> id_equals >> update_id;
            if (ss.fail() || keyword != "update" || table_keyword != "table" || set_keyword != "set" ||
                name_keyword != "name" || name_equals != "=" || email_keyword != "email" || email_equals != "=" ||
                where_keyword != "where" || id_keyword != "id" || id_equals != "=") {
                response["error"] = "Syntax error. Usage: update table set name = <name>, email = <email> where id = <id>";
                return;
            }
            if (name_value.length() >= COLUMN_NAME_SIZE || email_value.length() >= COLUMN_EMAIL_SIZE) {
                response["error"] = "Name or email too long.";
                return;
            }
            uint32_t row_num = session.current_table->btree->search(update_id);
            if (row_num != std::numeric_limits<uint32_t>::max() && !is_row_freed(session.current_table, row_num)) {
                Row old_row;
                deserialize_row(get_row_slot(session.current_table, row_num), old_row);
                if (txn_manager.isInTransaction(transaction_id)) {
                    txn_manager.logUpdate(transaction_id, session.current_table_name, row_num, old_row);
                }
                if (update_row(session.current_table, update_id, name_value, email_value)) {
                    response["message"] = "Row with ID " + std::to_string(update_id) + " updated.";
                } else {
                    response["error"] = "Row with ID " + std::to_string(update_id) + " not found or deleted.";
                }
            } else {
                response["message"] = "Row with ID " + std::to_string(update_id) + " not found or deleted.";
            }
            break;
        }
        case CommandType::BTREE_CMD: {
            if (!session.current_table) {
                response["error"] = "No table selected.";
                return;
            }
            std::stringstream btree_stream;
            btree_stream << "B-tree structure:\n";
            session.current_table->btree->printTree(session.current_table->btree->getRoot());
            response["message"] = btree_stream.str();
            break;
        }
        case CommandType::BEGIN_TRANSACTION: {
            if (!session.current_table) {
                response["error"] = "No table selected.";
                return;
            }
            LockType lock_type = (transaction_id == 2) ? LockType::SHARED : LockType::EXCLUSIVE;
            if (!txn_manager.beginTransaction(transaction_id, session.current_table, lock_type)) {
                response["error"] = "Transaction " + std::to_string(transaction_id) + " waiting for " +
                                   (lock_type == LockType::SHARED ? "shared" : "exclusive") + " lock on table '" + session.current_table_name + "'.";
                return;
            }
            response["message"] = "Transaction " + std::to_string(transaction_id) + " started.";
            break;
        }
        case CommandType::COMMIT: {
            if (txn_manager.commit(transaction_id)) {
                response["message"] = "Transaction " + std::to_string(transaction_id) + " committed successfully.";
            } else {
                response["error"] = "No active transaction " + std::to_string(transaction_id) + " to commit.";
            }
            break;
        }
        case CommandType::ROLLBACK: {
            if (!session.current_table) {
                response["error"] = "No table selected for rollback.";
                return;
            }
            if (txn_manager.rollback(transaction_id, session.current_table)) {
                response["message"] = "Transaction " + std::to_string(transaction_id) + " rolled back successfully.";
            } else {
                response["error"] = "No active transaction " + std::to_string(transaction_id) + " to rollback.";
            }
            break;
        }
        default:
            response["error"] = "Unrecognized command: " + command_input;
            break;
    }
}

int main() {
    DatabaseCatalog catalog("master");
    TransactionManager txn_manager("master");

    httplib::Server svr;
// Serve static HTML and other assets
svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
    std::ifstream file("index.html"); // your HTML file name, adjust path if needed
    if (file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        res.set_content(buffer.str(), "text/html");
    } else {
        res.status = 404;
        res.set_content("File not found", "text/plain");
    }
});

    // Endpoint to create a new session
   svr.Get("/api/create_session", [&](const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    static std::atomic<int> session_counter{0};
    std::string new_session_id = "sess_" + std::to_string(session_counter++);
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions[new_session_id] = Session{};
    }
    response["session_id"] = new_session_id;
    res.set_content(response.dump(), "application/json");
});


    // Endpoint to select a table
    svr.Post("/api/use_table", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json response;
        if (!req.has_param("session_id") || !req.has_param("table_name")) {
            response["error"] = "Missing session_id or table_name parameter";
            res.set_content(response.dump(), "application/json");
            return;
        }
        std::string session_id = req.get_param_value("session_id");
        std::string table_name = req.get_param_value("table_name");
        uint32_t transaction_id = req.has_param("transaction_id") ? std::stoi(req.get_param_value("transaction_id")) : 0;

        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(session_id);
        if (it == sessions.end()) {
            response["error"] = "Invalid session_id";
            res.set_content(response.dump(), "application/json");
            return;
        }
        Session& session = it->second;

        if (txn_manager.isInTransaction(transaction_id)) {
            response["error"] = "Cannot change table during an active transaction.";
            res.set_content(response.dump(), "application/json");
            return;
        }
        if (session.current_table) {
            catalog.closeTable(session.current_table);
            session.current_table = nullptr;
            session.current_table_name.clear();
        }
        Table* table = catalog.openTable(table_name);
        if (table) {
            session.current_table = table;
            session.current_table_name = table_name;
            txn_manager.recover(table);
            response["message"] = "Now using table '" + table_name + "' in database '" + session.current_db + "'.";
        } else {
            response["error"] = "Table '" + table_name + "' does not exist.";
        }
        res.set_content(response.dump(), "application/json");
    });

    // Endpoint to execute commands
    svr.Post("/api/execute", [&](const httplib::Request& req, httplib::Response& res) {
        nlohmann::json response;
        if (!req.has_param("session_id")) {
            response["error"] = "Missing session_id parameter";
            res.set_content(response.dump(), "application/json");
            return;
        }
        std::string session_id = req.get_param_value("session_id");
        uint32_t transaction_id = req.has_param("transaction_id") ? std::stoi(req.get_param_value("transaction_id")) : 0;
        std::string command = req.body;

        std::lock_guard<std::mutex> lock(sessions_mutex);
        auto it = sessions.find(session_id);
        if (it == sessions.end()) {
            response["error"] = "Invalid session_id";
            res.set_content(response.dump(), "application/json");
            return;
        }
        Session& session = it->second;

        handleCommand(command, catalog, txn_manager, session, transaction_id, response);
        res.set_content(response.dump(), "application/json");
    });

  // Cleanup endpoint
svr.Get("/api/close_session", [&catalog](const httplib::Request& req, httplib::Response& res) {
    nlohmann::json response;
    if (!req.has_param("session_id")) {
        response["error"] = "Missing session_id parameter";
        res.set_content(response.dump(), "application/json");
        return;
    }
    std::string session_id = req.get_param_value("session_id");
    std::lock_guard<std::mutex> lock(sessions_mutex);
    auto it = sessions.find(session_id);
    if (it != sessions.end()) {
        if (it->second.current_table) {
            catalog.closeTable(it->second.current_table);  // ✅ Now accessible
        }
        sessions.erase(it);
        response["message"] = "Session closed.";
    } else {
        response["error"] = "Invalid session_id";
    }
    res.set_content(response.dump(), "application/json");
});


    // Start the server
    std::cout << "Starting server on http://0.0.0.0:8080\n";
    svr.listen("0.0.0.0", 8080);

    // Cleanup
    std::lock_guard<std::mutex> lock(sessions_mutex);
    for (auto& it : sessions) {
    std::string session_id = it.first;
    Session& session = it.second;
    if (session.current_table) {
        catalog.closeTable(session.current_table);
    }
}

    txn_manager.rollbackAllActiveTransactions();
    return 0;
}