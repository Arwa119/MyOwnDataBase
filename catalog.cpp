#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <stdexcept>
#include "catalog.h"
#include "table.h"

using namespace std;

// Helper function to create directories
bool create_directories(const std::string& path) {
    std::string current = "";
    std::string delimiter = "/";
    
    #ifdef _WIN32
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

// Helper function to rename files
bool renameFile(const std::string& oldPath, const std::string& newPath) {
    #ifdef _WIN32
        return MoveFileA(oldPath.c_str(), newPath.c_str()) != 0;
    #else
        return rename(oldPath.c_str(), newPath.c_str()) == 0;
    #endif
}

// Helper function to remove a directory and its contents
bool remove_directory(const std::string& path) {
#ifdef _WIN32
    // Windows: Use SHFileOperation for recursive deletion
    SHFILEOPSTRUCTA op = {0};
    op.wFunc = FO_DELETE;
    // Double-null terminate the path
    std::string doubleNullTermPath = path + '\0';
    op.pFrom = doubleNullTermPath.c_str();
    op.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI; // Replaced FOF_NO_UI
    int result = SHFileOperationA(&op);
    return result == 0;
#else
    // Unix: Recursive deletion using dirent
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return errno == ENOENT; // Directory doesn't exist, consider it success
    }

    bool success = true;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string fullPath = path + "/" + entry->d_name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                if (!remove_directory(fullPath)) {
                    success = false;
                }
            } else {
                if (unlink(fullPath.c_str()) != 0) {
                    success = false;
                }
            }
        }
    }
    closedir(dir);
    
    if (success && rmdir(path.c_str()) != 0) {
        success = false;
    }
    return success;
#endif
}

DatabaseCatalog::DatabaseCatalog(const std::string& dbName) : currentDb(dbName) {
    create_directories(getDatabasePath(dbName));
    loadCatalog();
}

DatabaseCatalog::~DatabaseCatalog() {
    for (auto& pair : openTables) {
        closeTable(pair.second);
    }
    saveCatalog();
}

bool DatabaseCatalog::createDatabase(const std::string& dbName) {
    std::string dbPath = getDatabasePath(dbName);

    if (std::find(databases.begin(), databases.end(), dbName) != databases.end()) {
        std::cout << "Error: Database '" << dbName << "' already exists.\n";
        return false;
    }

    if (!create_directories(dbPath)) {
        std::cout << "Error: Could not create database directory.\n";
        return false;
    }

    databases.push_back(dbName);

    if (!saveCatalog()) {
        std::cout << "Error: Could not save catalog metadata.\n";
        return false;
    }

    return true;
}

bool DatabaseCatalog::dropDatabase(const std::string& dbName) {
    if (dbName == "master") {
        std::cout << "Error: Cannot drop the master database.\n";
        return false;
    }

    auto it = std::find(databases.begin(), databases.end(), dbName);
    if (it == databases.end()) {
        std::cout << "Error: Database '" << dbName << "' does not exist.\n";
        return false;
    }

    // Close all open tables in the database to be dropped
    std::string tempDb = currentDb;
    if (currentDb != dbName) {
        useDatabase(dbName); // Temporarily switch to load the database's tables
    }

    for (auto& pair : openTables) {
        closeTable(pair.second);
    }
    openTables.clear();
    tables.clear();

    // Delete the database directory and its contents
    std::string dbPath = getDatabasePath(dbName);
    try {
        if (!remove_directory(dbPath)) {
            std::cerr << "Error: Could not delete database directory '" << dbPath << "'.\n";
            if (currentDb == dbName) {
                useDatabase("master"); // Switch back to master if deletion fails
            } else {
                useDatabase(tempDb); // Restore original database
            }
            return false;
        }
    } catch (const std::exception& e) {
 panic:       std::cerr << "Error: Could not delete database directory '" << dbPath << "': " << e.what() << "\n";
        if (currentDb == dbName) {
            useDatabase("master");
        } else {
            useDatabase(tempDb);
        }
        return false;
    }

    // Remove the database from the list
    databases.erase(it);

    // Save the updated catalog
    if (currentDb == dbName) {
        useDatabase("master"); // Switch to master if current database was dropped
    } else {
        useDatabase(tempDb); // Restore original database
    }

    if (!saveCatalog()) {
        std::cout << "Error: Could not save catalog metadata after dropping database.\n";
        return false;
    }

    return true;
}

bool DatabaseCatalog::useDatabase(const std::string& dbName) {
    if (std::find(databases.begin(), databases.end(), dbName) == databases.end()) {
        std::cout << "Error: Database '" << dbName << "' does not exist.\n";
        return false;
    }
    
    for (auto& pair : openTables) {
        closeTable(pair.second);
    }
    openTables.clear();
    
    currentDb = dbName;
    loadCatalog();
    
    std::cout << "Using database: " << dbName << "\n";
    return true;
}

std::vector<std::string> DatabaseCatalog::listDatabases() {
    return databases;
}

bool DatabaseCatalog::saveCatalog() {
    std::ofstream file(getCatalogFilePath());
    if (!file.is_open()) {
        std::cerr << "Error: Could not open catalog file for writing.\n";
        return false;
    }
    
    file << "DATABASES\n";
    for (const auto& db : databases) {
        file << db << "\n";
    }
    
    file << "TABLES\n";
    for (const auto& table : tables) {
        file << table.tableName << "\n";
        file << table.filename << "\n";
        
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

bool DatabaseCatalog::loadCatalog() {
    std::string catalogPath = getCatalogFilePath();
    std::ifstream file(catalogPath);
    
    // If catalog doesn't exist, initialize fresh state
    if (!file.is_open()) {
        // Only initialize if this is the first time
        if (databases.empty()) {
            databases.push_back(currentDb);
        }
        tables.clear();
        return true;
    }

    // Temporary storage for loaded data
    std::vector<std::string> loadedDatabases;
    std::vector<TableMetadata> loadedTables;
    
    std::string line;
    std::string section;
    
    try {
        while (std::getline(file, line)) {
            // Skip empty lines
            if (line.empty()) continue;
            
            if (line == "DATABASES") {
                section = "DATABASES";
                continue;
            } else if (line == "TABLES") {
                section = "TABLES";
                continue;
            }
            
            if (section == "DATABASES") {
                loadedDatabases.push_back(line);
            } else if (section == "TABLES") {
                TableMetadata metadata;
                metadata.tableName = line;
                
                // Read filename
                if (!std::getline(file, line)) throw std::runtime_error("Unexpected EOF reading filename");
                metadata.filename = line;
                
                // Read columns
                if (!std::getline(file, line)) throw std::runtime_error("Unexpected EOF reading columns");
                
                // Parse columns
                std::istringstream columnStream(line);
                std::string columnDef;
                while (std::getline(columnStream, columnDef, ',')) {
                    size_t colonPos = columnDef.find(':');
                    if (colonPos == std::string::npos) {
                        throw std::runtime_error("Invalid column format: " + columnDef);
                    }
                    metadata.columnNames.push_back(columnDef.substr(0, colonPos));
                    metadata.columnTypes.push_back(columnDef.substr(colonPos + 1));
                }
                
                loadedTables.push_back(metadata);
            }
        }
        
        // Only update state if everything succeeded
        databases = loadedDatabases;
        tables = loadedTables;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Catalog load error: " << e.what() << std::endl;
        return false;
    }
}

bool DatabaseCatalog::createTable(const std::string& tableName,
                                 const std::vector<std::string>& columnNames,
                                 const std::vector<std::string>& columnTypes) {
    for (const auto& table : tables) {
        if (table.tableName == tableName) {
            std::cout << "Error: Table '" << tableName << "' already exists.\n";
            return false;
        }
    }
    
    TableMetadata metadata;
    metadata.tableName = tableName;
    metadata.filename = getTablePath(tableName);
    metadata.columnNames = columnNames;
    metadata.columnTypes = columnTypes;
    
    tables.push_back(metadata);
    
    Table* table = db_open(metadata.filename);
    openTables[tableName] = table;
    
    saveCatalog();
    std::cout << "Table created: " << tableName << "\n";
    return true;
}

bool DatabaseCatalog::dropTable(const std::string& tableName) {
    auto it = std::find_if(tables.begin(), tables.end(),
                          [&](const TableMetadata& t) { return t.tableName == tableName; });
    
    if (it == tables.end()) {
        std::cout << "Error: Table '" << tableName << "' does not exist.\n";
        return false;
    }
    
    if (openTables.find(tableName) != openTables.end()) {
        closeTable(openTables[tableName]);
    }
    
    std::string filename = it->filename;
    #ifdef _WIN32
        bool deleted = (DeleteFileA(filename.c_str()) != 0);
    #else
        bool deleted = (unlink(filename.c_str()) == 0);
    #endif
    
    if (!deleted) {
        std::cerr << "Warning: Could not delete table file.\n";
    }
    
    tables.erase(it);
    
    saveCatalog();
    return true;
}

std::string DatabaseCatalog::getTableName(Table* tablePtr) const {
    for (const auto& pair : openTables) {
        if (pair.second == tablePtr) {
            return pair.first;
        }
    }
    return "";
}

bool DatabaseCatalog::alterTableName(const std::string& oldName, const std::string& newName) {
    auto it = std::find_if(tables.begin(), tables.end(),
                          [&](const TableMetadata& t) { return t.tableName == oldName; });
    
    if (it == tables.end()) {
        std::cout << "Error: Table '" << oldName << "' does not exist.\n";
        return false;
    }
    
    auto newIt = std::find_if(tables.begin(), tables.end(),
                             [&](const TableMetadata& t) { return t.tableName == newName; });
    
    if (newIt != tables.end()) {
        std::cout << "Error: Table '" << newName << "' already exists.\n";
        return false;
    }
    
    if (openTables.find(oldName) != openTables.end()) {
        Table* table = openTables[oldName];
        openTables.erase(oldName);
        openTables[newName] = table;
    }
    
    std::string oldFilename = it->filename;
    std::string newFilename = getTablePath(newName);
    
    try {
        if (!renameFile(oldFilename, newFilename)) {
            std::cout << "Error renaming table file\n";
            return false;
        }
    } catch (...) {
        std::cout << "Error renaming table file\n";
        return false;
    }
    
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

Table* DatabaseCatalog::openTable(const std::string& tableName) {
    auto it = std::find_if(tables.begin(), tables.end(),
                           [&](const TableMetadata& t) { return t.tableName == tableName; });
    if (it == tables.end()) {
        std::cout << "Error: Table '" << tableName << "' does not exist.\n";
        return nullptr;
    }

    if (openTables.find(tableName) != openTables.end()) {
        return openTables[tableName];
    }

    Table* table = db_open(it->filename);
    openTables[tableName] = table;
    return table;
}

void DatabaseCatalog::closeTable(Table* table) {
    if (!table) return;

    auto it = openTables.begin();
    while (it != openTables.end()) {
        if (it->second == table) {
            db_close(table);
            it = openTables.erase(it);
            return;
        } else {
            ++it;
        }
    }
}

std::string DatabaseCatalog::getDatabasePath(const std::string& dbName) {
    return "./databases/" + dbName;
}

std::string DatabaseCatalog::getTablePath(const std::string& tableName) {
    return getDatabasePath(currentDb) + "/" + tableName + ".db";
}

std::string DatabaseCatalog::getCatalogFilePath() {
    return getDatabasePath(currentDb) + "/catalog.db";
}

TableMetadata DatabaseCatalog::getTableMetadata(const std::string& tableName) {
    auto it = std::find_if(tables.begin(), tables.end(),
                           [&](const TableMetadata& t) { return t.tableName == tableName; });
    if (it != tables.end()) {
        return *it;
    }
    return TableMetadata();
}