#ifndef CATALOG_H
#define CATALOG_H
#include <string>
#include <vector>
#include <map>
#include "table.h"

struct TableMetadata {
    std::string tableName;
    std::string filename;  // Physical file storing the table
    std::vector<std::string> columnNames;
    std::vector<std::string> columnTypes;
};

class DatabaseCatalog {
public:
    DatabaseCatalog(const std::string& dbName);
    ~DatabaseCatalog();
    // Database operations
    std::string getCurrentDatabase() const;
    bool dropDatabase(const std::string& dbName);
    bool createDatabase(const std::string& dbName);
    // bool dropDatabase(const std::string& dbName);
    bool useDatabase(const std::string& dbName);
    std::vector<std::string> listDatabases();
    bool saveCatalog();
    bool loadCatalog();
    // Table operations
    // bool createTable(const std::string& tableName, 
    //                  const std::vector<std::string>& columnNames,
    //                  const std::vector<std::string>& columnTypes);
    // Add this alongside your existing createTable declaration
bool createTable(const std::string& dbName, const std::string& tableName, 
                const std::string& tablePath, const std::string& schema);
    bool dropTable(const std::string& tableName);
    std::string getTableName(Table* tablePtr) const;
    bool alterTableName(const std::string& oldName, const std::string& newName);
    std::vector<std::string> listTables();
    TableMetadata getTableMetadata(const std::string& tableName);
    
    // Get a table object for operations
    Table* openTable(const std::string& tableName);
    void closeTable(Table* table);

private:
      std::string currentDb = "master";
    std::map<std::string, Table*> openTables;
    
    // Save and load catalog metadata
    // bool saveCatalog();
    // bool loadCatalog();
    
    // Path helpers
    std::string getDatabasePath(const std::string& dbName);
    std::string getTablePath(const std::string& tableName);
    std::string getCatalogFilePath();
   
    // Catalog data structures
    std::vector<std::string> databases;
    std::vector<TableMetadata> tables;
};

#endif