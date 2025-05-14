// Add these headers at the top of your file instead of <filesystem>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream> // Added to fix the issue with std::istringstream
#include "catalog.h"
using namespace std;

// Add this function to your DatabaseCatalog class or as a standalone function
bool create_directories(const std::string& path) {
    std::string current = "";
    std::string delimiter = "/";
    
    #ifdef _WIN32
        // Also handle Windows backslashes
        std::string path_copy = path;
        std::replace(path_copy.begin(), path_copy.end(), '\\', '/');
        std::string dir_path = path_copy;
    #else
        std::string dir_path = path;
    #endif
    
    size_t pos = 0;
    bool result = true;
    
    while ((pos = dir_path.find(delimiter, pos)) != std::string::npos) {
        current = dir_path.substr(0, pos);
        if (!current.empty()) {
            #ifdef _WIN32
                result = (_mkdir(current.c_str()) == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
            #else
                result = (mkdir(current.c_str(), 0777) == 0 || errno == EEXIST);
            #endif
            
            if (!result) {
                return false;
            }
        }
        pos += delimiter.length();
    }
    
    if (!dir_path.empty()) {
        #ifdef _WIN32
            result = (_mkdir(dir_path.c_str()) == 0 || GetLastError() == ERROR_ALREADY_EXISTS);
        #else
            result = (mkdir(dir_path.c_str(), 0777) == 0 || errno == EEXIST);
        #endif
    }
    
    return result;
}
DatabaseCatalog::DatabaseCatalog(const std::string& dbName) : currentDb(dbName) {
    // Create the database directory if it doesn't exist
    create_directories(getDatabasePath(dbName));
    // Load catalog metadata
    loadCatalog();
}
bool DatabaseCatalog::saveCatalog() {
    std::ofstream file(getCatalogFilePath());
    if (!file.is_open()) {
        std::cerr << "Error: Could not open catalog file for writing.\n";
        return false;
    }
    
    // Write databases section
    file << "DATABASES\n";
    for (const auto& db : databases) {
        file << db << "\n";
    }
    
    // Write tables section
    file << "TABLES\n";
    for (const auto& table : tables) {
        file << table.tableName << "\n";
        file << table.filename << "\n";
        
        // Write columns in format name:type,name:type,...
        for (size_t i = 0; i < table.columnNames.size(); i++) {
            file << table.columnNames[i] << ":" << table.columnTypes[i];
            if (i < table.columnNames.size() - 1) {
                file << ",";
            }
        }
        file << "\n";
    }
    
    file.close();
    return true;
}

std::string DatabaseCatalog::getCatalogFilePath() {
    return getDatabasePath(currentDb) + "/catalog.db";
}

std::string DatabaseCatalog::getDatabasePath(const std::string& dbName) {
    return "./databases/" + dbName;
}

std::string DatabaseCatalog::getTablePath(const std::string& tableName) {
    return getDatabasePath(currentDb) + "/" + tableName + ".db";
}
std::string DatabaseCatalog::getTableName(Table* tablePtr) const {
    // Search through open tables to find the one that matches the pointer
    for (const auto& pair : openTables) {
        if (pair.second == tablePtr) {
            return pair.first;
        }
    }
    
    // If the table isn't found in open tables
    return "";
}
DatabaseCatalog::~DatabaseCatalog() {
    // Close all open tables
    for (auto& pair : openTables) {
        db_close(pair.second);
    }
    // Save catalog changes
    saveCatalog();
}

bool DatabaseCatalog::createDatabase(const std::string& dbName) {
    // ...
    std::string dbPath = getDatabasePath(dbName);
    if (!create_directories(dbPath)) {
        std::cout << "Error: Could not create database directory.\n";
        return false;
    }
     
    // Create the database directory
   dbPath = getDatabasePath(dbName);
    if (!create_directories(dbPath)) {
        std::cout << "Error: Could not create database directory.\n";
        return false;
    }
    
    // Add to the list of databases
    databases.push_back(dbName);
    
    // Save the up
    // ...
}

bool DatabaseCatalog::useDatabase(const std::string& dbName) {
    // Check if database exists
    if (std::find(databases.begin(), databases.end(), dbName) == databases.end()) {
        std::cout << "Error: Database '" << dbName << "' does not exist.\n";
        return false;
    }
    
    // Close all open tables from current database
    for (auto& pair : openTables) {
        db_close(pair.second);
    }
    openTables.clear();
    
    // Switch to new database
    currentDb = dbName;
    loadCatalog();  // Load tables for the new database
    
    std::cout << "Using database: " << dbName << "\n";
    return true;
}
bool DatabaseCatalog::loadCatalog() {

 std::string catalogPath = getCatalogFilePath();
    
    // If catalog file doesn't exist, initialize fresh database
    if (!std::ifstream(catalogPath)) {
        tables.clear();
        return true; // Fresh database with no tables
    }
    std::ifstream file(getCatalogFilePath());
  
    std::string line;
    std::string section = "";
    
    while (std::getline(file, line)) {
        if (line == "DATABASES") {
            section = "DATABASES";
            continue;
        } else if (line == "TABLES") {
            section = "TABLES";
            continue;
        }
        
        if (section == "DATABASES") {
            databases.push_back(line);
        } else if (section == "TABLES") {
            // Read table name
            std::string tableName = line;
            
            // Read filename
            std::getline(file, line);
            std::string filename = line;
            
            // Read columns
            std::getline(file, line);
            std::vector<std::string> columnNames;
            std::vector<std::string> columnTypes;
            
            std::string columnInfo = line;
            std::string token;
            std::istringstream columnStream(columnInfo);
            
            while (std::getline(columnStream, token, ',')) {
                size_t pos = token.find(':');
                if (pos != std::string::npos) {
                    std::string name = token.substr(0, pos);
                    std::string type = token.substr(pos + 1);
                    columnNames.push_back(name);
                    columnTypes.push_back(type);
                }
            }
            
            TableMetadata metadata;
            metadata.tableName = tableName;
            metadata.filename = filename;
            metadata.columnNames = columnNames;
            metadata.columnTypes = columnTypes;
            
            tables.push_back(metadata);
        }
    }
    
    file.close();
    return true;
}



bool DatabaseCatalog::dropTable(const std::string& tableName) {
    // Find the table in our metadata
    auto it = std::find_if(tables.begin(), tables.end(), 
                          [&](const TableMetadata& t) { return t.tableName == tableName; });
    
    if (it == tables.end()) {
        std::cout << "Error: Table '" << tableName << "' does not exist.\n";
        return false;
    }
    
    // Close the table if it's open
    if (openTables.find(tableName) != openTables.end()) {
        db_close(openTables[tableName]);
        openTables.erase(tableName);
    }
    
    // Delete the file
    std::string filename = it->filename;
    #ifdef _WIN32
        bool deleted = (DeleteFileA(filename.c_str()) != 0);
    #else
        bool deleted = (unlink(filename.c_str()) == 0);
    #endif
    
    if (!deleted) {
        std::cerr << "Warning: Could not delete table file.\n";
        // Continue anyway - we'll still remove the metadata
    }
    
    // Remove from tables list
    tables.erase(it);
    
    // Save updated catalog
    saveCatalog();
    return true;
}

// Add this implementation after your existing createTable method
bool DatabaseCatalog::createTable(const std::string& dbName, const std::string& tableName, 
                                 const std::string& tablePath, const std::string& schema) {
    // Check if table already exists
    for (const auto& table : tables) {
        if (table.tableName == tableName) {
            std::cout << "Error: Table '" << tableName << "' already exists.\n";
            return false;
        }
    }
    
    // Parse schema string to extract column names and types
    std::vector<std::string> columnNames;
    std::vector<std::string> columnTypes;
    
    if (!schema.empty()) {
        std::string token;
        std::istringstream schemaStream(schema);
        
        while (std::getline(schemaStream, token, ',')) {
            size_t pos = token.find(':');
            if (pos != std::string::npos) {
                std::string name = token.substr(0, pos);
                std::string type = token.substr(pos + 1);
                columnNames.push_back(name);
                columnTypes.push_back(type);
            }
        }
    }
    
    // Create table metadata
    TableMetadata metadata;
    metadata.tableName = tableName;
    metadata.filename = tablePath; // Use the provided path
    metadata.columnNames = columnNames;
    metadata.columnTypes = columnTypes;
    
    // Add to tables list
    tables.push_back(metadata);
    
    // Create empty table file
    Table* table = db_open(metadata.filename);
    openTables[tableName] = table;
    
    saveCatalog();
    std::cout << "Table created: " << tableName << "\n";
    return true;
}
bool renameFile(const std::string& oldPath, const std::string& newPath) {
    #ifdef _WIN32
        return MoveFileA(oldPath.c_str(), newPath.c_str()) != 0;
    #else
        return rename(oldPath.c_str(), newPath.c_str()) == 0;
    #endif
}

std::string DatabaseCatalog::getCurrentDatabase() const {
    return currentDb;
}
bool DatabaseCatalog::dropDatabase(const std::string& dbName) {
    // Check if database exists
    auto it = std::find(databases.begin(), databases.end(), dbName);
    if (it == databases.end()) {
        std::cout << "Error: Database '" << dbName << "' does not exist.\n";
        return false;
    }
    
    // Can't drop current database
    if (currentDb == dbName) {
        std::cout << "Error: Cannot drop currently selected database.\n";
        return false;
    }
    
    // Get database path
    std::string dbPath = getDatabasePath(dbName);
    
    // Remove directory and all contents
    #ifdef _WIN32
        std::string cmd = "rmdir /s /q \"" + dbPath + "\"";
    #else
        std::string cmd = "rm -rf \"" + dbPath + "\"";
    #endif
    
    if (system(cmd.c_str()) != 0) {
        std::cout << "Error: Failed to remove database directory.\n";
        return false;
    }
    
    // Remove from databases list
    databases.erase(it);
    
    // Save catalog
    saveCatalog();
    return true;
}
bool DatabaseCatalog::alterTableName(const std::string& oldName, const std::string& newName) {
    // Find the table
    auto it = std::find_if(tables.begin(), tables.end(), 
                          [&](const TableMetadata& t) { return t.tableName == oldName; });
    
    if (it == tables.end()) {
        std::cout << "Error: Table '" << oldName << "' does not exist.\n";
        return false;
    }
    
    // Check if new name already exists
    auto newIt = std::find_if(tables.begin(), tables.end(), 
                             [&](const TableMetadata& t) { return t.tableName == newName; });
    
    if (newIt != tables.end()) {
        std::cout << "Error: Table '" << newName << "' already exists.\n";
        return false;
    }
    
    // Close the table if it's open
    if (openTables.find(oldName) != openTables.end()) {
        db_close(openTables[oldName]);
        openTables.erase(oldName);
    }
    
    // Get old and new filenames
    std::string oldFilename = it->filename;
    std::string newFilename = getTablePath(newName);
    
    // Rename file
   try {
    if (!renameFile(oldFilename, newFilename)) {
        std::cout << "Error renaming table file\n";
        return false;
    }
} catch (...) {
    std::cout << "Error renaming table file\n";
    return false;
}
    // Update metadata
    it->tableName = newName;
    it->filename = newFilename;
    
    saveCatalog();
    std::cout << "Table renamed: " << oldName << " -> " << newName << "\n";
    return true;
}

std::vector<std::string> DatabaseCatalog::listTables() {
    std::vector<std::string> tableNames;
    for (const auto& table : tables) {
        tableNames.push_back(table.tableName);
    }
    return tableNames;
}
