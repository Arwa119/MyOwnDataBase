#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"
#include <stdexcept>
#include <vector>
#include <string>

class ParserException : public std::runtime_error {
public:
    ParserException(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    Parser(const std::vector<Token>& tokens) : tokens(tokens), current(0) {}

    // Parse methods for different SQL commands
    void parseCreateDatabase(std::string& db_name);
    void parseDropDatabase(std::string& db_name);
    void parseUseDatabase(std::string& db_name);
    void parseCreateTable(std::string& table_name, std::vector<std::string>& column_names, std::vector<std::string>& column_types);
    void parseDropTable(std::string& table_name);
    void parseUseTable(std::string& table_name);
    void parseInsert(int& id, std::string& name, std::string& email);
    void parseSelect(bool& has_where, int& where_id);
    void parseDelete(std::string& table_name, bool& has_where, int& where_id);
    void parseUpdate(std::string& table_name, int& id, std::string& name, std::string& email);
    void parseAlterTable(std::string& old_name, std::string& new_name);
    void parseShowTables();
    void parseBeginTransaction();
    void parseCommit();
    void parseRollback();

private:
    const std::vector<Token>& tokens;
    size_t current;

    // Helper methods
    Token peek();
    Token advance();
    bool isAtEnd();
    bool check(TokenType type);
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& message);
    bool matchKeyword(const std::string& keyword);
    Token consumeKeyword(const std::string& keyword, const std::string& message);
    Token consumeIdentifier(const std::string& message);
    Token consumeNumber(const std::string& message);
    Token consumeString(const std::string& message);
};

#endif