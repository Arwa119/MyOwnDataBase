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

int main() {
    // Open or create the database table
    Table* table = db_open("mydb.db");

    std::string input;
    // Main command loop
    while (true) {
        std::cout << "db > ";
        std::getline(std::cin, input);

        // Handle the exit command
        if (input == ".exit") {
            db_close(table); // Close the database before exiting
            std::cout << "Exiting...\n";
            break;
        }
        // Parse the user input command
        CommandType command = parse_command(input);
        switch (command) {
            case CommandType::INSERT: {
                std::stringstream ss(input);
                std::string keyword;
                int id;
                std::string name, email;
                // Parse insert command arguments
                ss >> keyword >> id >> name >> email;
                // Check for syntax errors or missing arguments
                if (ss.fail()) {
                    std::cout << "Syntax error. Usage: insert <id> <name> <email>\n";
                    ss.clear();
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    break;
                }
                // Check for extra input after the expected arguments
                std::string remaining;
                if (ss >> remaining) {
                     std::cout << "Syntax error: Extra input after email.\n Usage: insert <id> <name> <email>\n";
                     break;
                }
                // Validate name and email lengths
                if (name.length() >= COLUMN_NAME_SIZE) {
                    std::cout << "Error: Name is too long (max " << COLUMN_NAME_SIZE - 1 << " characters).\n";
                    break;
                }
                if (email.length() >= COLUMN_EMAIL_SIZE) {
                     std::cout << "Error: Email is too long (max " << COLUMN_EMAIL_SIZE - 1 << " characters).\n";
                    break;
                }
                // Check for duplicate ID using the B-tree index
                uint32_t existing_row_num = table->btree->search(id);
                if (existing_row_num != std::numeric_limits<uint32_t>::max()) {
                    // If a row with the same ID exists, check if it's deleted
                    if (!is_row_freed(table, existing_row_num)) {
                         std::cout << "Error: Duplicate ID. Row with ID " << id << " already exists at row number " << existing_row_num << ".\n";
                         break;
                    } else {
                         std::cout << "Warning: Duplicate ID " << id << " found in index but marked as deleted at row number " << existing_row_num << ". Inserting as a new row.\n";
                    }
                }
                // Always append to the end of the allocated rows for now.
                uint32_t row_to_insert_at = table->num_rows;
                // Check if the table is full
                if (row_to_insert_at >= TABLE_MAX_ROWS) {
                    std::cout << "Error: Table full.\n";
                    break;
                }
                // Create and serialize the new row
                Row row = create_row(id, name, email); // create_row initializes is_deleted to false
                void* row_slot = get_row_slot(table, row_to_insert_at);
                serialize_row(row, row_slot);
                table->num_rows++; // Increment total allocated row count
                // Insert the new row's ID and row number into the B-tree index
                table->btree->insert(id, row_to_insert_at);
                // Mark the page containing the new row as dirty so it gets flushed to disk
                uint32_t page_num;
                 if (row_to_insert_at < ROWS_PER_PAGE_PAGE_0) {
                     page_num = 0;
                 } else {
                     uint32_t row_num_after_page_0 = row_to_insert_at - ROWS_PER_PAGE_PAGE_0;
                     page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
                 }
                table->pager->pages_dirty[page_num] = true;
                std::cout << "Row inserted.\n";
                break;
            }
            case CommandType::SELECT: {
                std::stringstream ss(input);
                std::string keyword;
                std::string next_word;
                ss >> keyword;
                ss >> next_word;
                // Handle full table scan (select command without 'where')
                if (ss.fail()) {
                    // Iterate through all allocated rows
                    if (table->num_rows == 0) {
                        std::cout << "Table is empty.\n";
                    } else {
                        std::cout << "Retrieving all rows....:\n";
                        uint32_t rows_displayed = 0;
                        for (uint32_t i = 0; i < table->num_rows; i++) {
                             // Check if the row is marked as deleted
                             bool is_freed = is_row_freed(table, i);
                            if (!is_freed) { // Only print if not deleted
                                void* row_slot = get_row_slot(table, i);
                                Row row;
                                deserialize_row(row_slot, row);
                                print_row(row);
                                rows_displayed++;
                            }
                        }
                         if (rows_displayed == 0 && table->num_rows > 0) {
                             std::cout << "All rows currently allocated are deleted.\n";
                         } else if (table->num_rows == 0) {
                             // Already handled by the initial check, but good to be explicit
                         }
                    }
                } else if (next_word == "where") { // Handle select with a where clause
                    std::string id_keyword;
                    std::string equals_sign;
                    int search_id;
                    // Parse the where clause arguments
                    ss >> id_keyword >> equals_sign >> search_id;
                    // Check for syntax errors in the where clause
                    if (ss.fail() || id_keyword != "id" || equals_sign != "=") {
                        std::cout << "Syntax error. Usage: select where id = <id>\n";
                        ss.clear();
                        ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        break;
                    }

                     // Check for extra input after the where clause
                     std::string remaining;
                    if (ss >> remaining) {
                        std::cout << "Syntax error: Extra input after ID.\n Usage: select where id = <id>\n";
                        break;
                    }

                    // Search the B-tree for the row number associated with the ID
                    uint32_t found_row_num = table->btree->search(search_id);

                    // If the ID is found in the index
                    if (found_row_num != std::numeric_limits<uint32_t>::max()) {
                         // Check if the row is marked as deleted
                         if (!is_row_freed(table, found_row_num)) {
                            std::cout << "Found row with ID " << search_id << " at row number " << found_row_num << ":\n";
                            void* row_slot = get_row_slot(table, found_row_num);
                            Row row;
                            deserialize_row(row_slot, row);
                            print_row(row);
                         } else {
                             std::cout << "Row with ID " << search_id << " found in index but marked as deleted.\n";
                         }
                    } else {
                        std::cout << "Row with ID " << search_id << " not found.\n";
                    }

                } else {
                    std::cout << "Syntax error after 'select'. Usage: select or select where id = <id>\n";
                    ss.clear();
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                }
                break;
            }
            case CommandType::DELETE: {
                std::stringstream ss(input);
                std::string keyword;
                std::string from_keyword;
                std::string table_name;
                std::string where_keyword;
                std::string id_keyword;
                std::string equals_sign;
                int delete_id;
                // Parse the delete command arguments
                ss >> keyword >> from_keyword >> table_name >> where_keyword >> id_keyword >> equals_sign >> delete_id;

                // Check for syntax errors
                if (ss.fail() || keyword != "delete" || from_keyword != "from" || table_name != "table" ||
                    where_keyword != "where" || id_keyword != "id" || equals_sign != "=") {
                    std::cout << "Syntax error. Usage: delete from table where id = <id>\n";
                    ss.clear();
                    ss.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    break;
                }
                 // Check for extra input after the ID
                 std::string remaining;
                if (ss >> remaining) {
                    std::cout << "Syntax error: Extra input after ID.\n Usage: delete from table where id = <id>\n";
                    break;
                }
                // Search the B-tree for the row number associated with the ID
                uint32_t row_num_to_delete = table->btree->search(delete_id);
                // If the ID is found in the index
                if (row_num_to_delete != std::numeric_limits<uint32_t>::max()) {
                    // Attempt to remove the ID from the B-tree index
                    bool removed_from_btree = table->btree->remove(delete_id);
                    if (removed_from_btree) {
                        // Get the row slot and mark the row as deleted
                        void* row_slot = get_row_slot(table, row_num_to_delete);
                        Row row;
                        deserialize_row(row_slot, row);
                        row.is_deleted = true; // Mark as deleted
                        serialize_row(row, row_slot); // Write the updated row back
                        // Mark the page containing the deleted row as dirty
                        uint32_t page_num;
                         if (row_num_to_delete < ROWS_PER_PAGE_PAGE_0) {
                             page_num = 0;
                         } else {
                             uint32_t row_num_after_page_0 = row_num_to_delete - ROWS_PER_PAGE_PAGE_0;
                             page_num = 1 + (row_num_after_page_0 / ROWS_PER_PAGE_OTHER_PAGES);
                         }
                        table->pager->pages_dirty[page_num] = true;
                        std::cout << "Row with ID " << delete_id << " deleted.\n";
                    } else {
                        std::cerr << "Error: Key found in search but failed to remove from B-tree.\n";
                    }
                } else {
                    std::cout << "Row with ID " << delete_id << " not found.\n";
                }
                break;
            }
            case CommandType::BTREE_CMD: {
                // Print the structure of the B-tree
                std::cout << "Printing B-tree structure:\n";
                table->btree->printTree(table->btree->getRoot());
                break;
            }
            case CommandType::CREATE_DB: {
    std::stringstream ss(input);
    std::string create_keyword, db_keyword, db_name;
    ss >> create_keyword >> db_keyword >> db_name;
    
    if (ss.fail() || create_keyword != "create" || db_keyword != "database") {
        std::cout << "Syntax error. Usage: create database <database_name>\n";
        break;
    }
    
    // Check for extra input
    std::string remaining;
    if (ss >> remaining) {
        std::cout << "Syntax error: Extra input after database name.\n";
        break;
    }
    
    DatabaseCatalog catalog("master"); // Use a default catalog
    if (catalog.createDatabase(db_name)) {
        std::cout << "Database '" << db_name << "' created successfully.\n";
    } else {
        std::cout << "Failed to create database '" << db_name << "'.\n";
    }
    break;
}
case CommandType::USE_DB: {
    std::stringstream ss(input);
    std::string use_keyword, db_name;
    ss >> use_keyword >> db_name;
    
    if (ss.fail() || use_keyword != "use") {
        std::cout << "Syntax error. Usage: use <database_name>\n";
        break;
    }
    
    // Check for extra input
    std::string remaining;
    if (ss >> remaining) {
        std::cout << "Syntax error: Extra input after database name.\n";
        break;
    }
    
    // Close current table before switching databases
    db_close(table);
    table = nullptr;
    
    DatabaseCatalog catalog("master");
    if (catalog.useDatabase(db_name)) {
        // We would typically reopen tables here or set the current database
        std::cout << "Now using database '" << db_name << "'.\n";
    } else {
        // If database switch fails, reopen the default table
        table = db_open("mydb.db");
    }
    break;
    
}
case CommandType::CREATE_TABLE: {
    std::stringstream ss(input);
    std::string create_keyword, table_keyword, table_name;
    ss >> create_keyword >> table_keyword >> table_name;
    
    if (ss.fail() || create_keyword != "create" || table_keyword != "table") {
        std::cout << "Syntax error. Usage: create table <table_name> (<column_name> <type>, ...)\n";
        break;
    }
    
    // Parse the column definitions
    std::string columns_str;
    std::getline(ss, columns_str);
    
    // Extracting column names and types from the string
    std::vector<std::string> column_names;
    std::vector<std::string> column_types;
    
    // Simple parsing logic for column definitions
    size_t open_paren = columns_str.find('(');
    size_t close_paren = columns_str.find_last_of(')');
    
    if (open_paren == std::string::npos || close_paren == std::string::npos) {
        std::cout << "Syntax error: Missing parentheses in column definitions.\n";
        break;
    }
    
    std::string columns_content = columns_str.substr(open_paren + 1, close_paren - open_paren - 1);
    std::stringstream columns_stream(columns_content);
    std::string column_def;
    
    while (std::getline(columns_stream, column_def, ',')) {
        // Trim whitespace
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
    
    // If we have valid column definitions, create the table
    if (!column_names.empty() && column_names.size() == column_types.size()) {
        DatabaseCatalog catalog("master"); // or use the current database
        if (catalog.createTable(table_name, column_names, column_types)) {
            std::cout << "Table '" << table_name << "' created successfully.\n";
        } else {
            std::cout << "Failed to create table '" << table_name << "'.\n";
        }
    } else {
        std::cout << "Error: No valid column definitions provided.\n";
    }
    break;
}
case CommandType::ALTER_TABLE: {
    std::stringstream ss(input);
    std::string alter_keyword, table_keyword, table_name, rename_keyword, to_keyword, new_table_name;
    ss >> alter_keyword >> table_keyword >> table_name >> rename_keyword >> to_keyword >> new_table_name;
    
    if (ss.fail() || alter_keyword != "alter" || table_keyword != "table" || 
        rename_keyword != "rename" || to_keyword != "to") {
        std::cout << "Syntax error. Usage: alter table <table_name> rename to <new_table_name>\n";
        break;
    }
    
    // Check for extra input
    std::string remaining;
    if (ss >> remaining) {
        std::cout << "Syntax error: Extra input after new table name.\n";
        break;
    }
    
    DatabaseCatalog catalog("master"); // or use the current database
    if (catalog.alterTableName(table_name, new_table_name)) {
        std::cout << "Table renamed from '" << table_name << "' to '" << new_table_name << "' successfully.\n";
    } else {
        std::cout << "Failed to rename table from '" << table_name << "' to '" << new_table_name << "'.\n";
    }
    break;
}

case CommandType::SHOW_TABLES: {
    std::stringstream ss(input);
    std::string show_keyword, tables_keyword;
    ss >> show_keyword >> tables_keyword;
    
    if (ss.fail() || show_keyword != "show" || tables_keyword != "tables") {
        std::cout << "Syntax error. Usage: show tables\n";
        break;
    }
    
    // Check for extra input
    std::string remaining;
    if (ss >> remaining) {
        std::cout << "Syntax error: Extra input after 'tables'.\n";
        break;
    }
    
    DatabaseCatalog catalog("master"); // or use the current database
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
    std::stringstream ss(input);
    std::string drop_keyword, table_keyword, table_name;
    ss >> drop_keyword >> table_keyword >> table_name;
    
    if (ss.fail() || drop_keyword != "drop" || table_keyword != "table") {
        std::cout << "Syntax error. Usage: drop table <table_name>\n";
        break;
    }
    
    // Check for extra input
    std::string remaining;
    if (ss >> remaining) {
        std::cout << "Syntax error: Extra input after table name.\n";
        break;
    }
    
    DatabaseCatalog catalog("master"); // or use the current database
    
    // If the table we're dropping is the current open table, close it first
    if (table != nullptr && catalog.getTableName(table) == table_name) {
        db_close(table);
        table = nullptr;
    }

    if (catalog.dropTable(table_name)) {
        std::cout << "Table '" << table_name << "' dropped successfully.\n";
    } else {
        std::cout << "Failed to drop table '" << table_name << "'.\n";
    }

    // If we closed our working table, reopen the default one
    if (table == nullptr) {
        table = db_open("mydb.db");
    }
    break;
}
   
            case CommandType::UNKOWN:
            default:
                std::cout << "Unrecognized command: " << input << "\n";
                break;
        }
    }
    return 0;
}
