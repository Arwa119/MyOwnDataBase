#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include "tokenizer.h"
using namespace MyTokenizer;  

#undef DELETE
enum class CommandType {
    INSERT,
    SELECT,
    DELETE,
    UPDATE,
    BTREE_CMD,
    CREATE_DB,
    DROP_DB,
    USE_DB,
    CREATE_TABLE,
    ALTER_TABLE,
    SHOW_TABLES,
    DROP_TABLE,
    USE_TABLE,
    BEGIN_TRANSACTION,
    COMMIT,
    ROLLBACK,
    UNKNOWN
};

CommandType parse_command(const std::string& input) {
    // Create tokenizer and get tokens
    Tokenizer tokenizer;
    std::vector<Token> tokens = tokenizer.tokenize(input);
    
    // Need at least one token to determine command type
    if (tokens.empty() || tokens[0].type != TokenType::KEYWORD) {
        return CommandType::UNKNOWN;
    }
    
    std::string first_keyword = tokens[0].value;
    
    if (first_keyword == "insert") {
        return CommandType::INSERT;
    }
    else if (first_keyword == "select") {
        return CommandType::SELECT;
    }
    else if (first_keyword == "delete") {
        return CommandType::DELETE;
    }
    else if (first_keyword == "update") {
        return CommandType::UPDATE;
    }
    else if (first_keyword == "create") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD) {
            if (tokens[1].value == "database") {
                return CommandType::CREATE_DB;
            }
            else if (tokens[1].value == "table") {
                return CommandType::CREATE_TABLE;
            }
        }
    }
    else if (first_keyword == "use") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD && tokens[1].value == "table") {
            return CommandType::USE_TABLE;
        }
        return CommandType::USE_DB;
    }
    else if (first_keyword == "show") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD && tokens[1].value == "tables") {
            return CommandType::SHOW_TABLES;
        }
    }
    else if (first_keyword == "drop") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD) {
            if (tokens[1].value == "table") {
                return CommandType::DROP_TABLE;
            }
            else if (tokens[1].value == "database") {
                return CommandType::DROP_DB;
            }
        }
    }
    else if (first_keyword == "alter") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD && tokens[1].value == "table") {
            return CommandType::ALTER_TABLE;
        }
    }
    else if (first_keyword == "begin") {
        if (tokens.size() > 2 && tokens[1].type == TokenType::KEYWORD && tokens[1].value == "transaction") {
            return CommandType::BEGIN_TRANSACTION;
        }
    }
    else if (first_keyword == "commit") {
        return CommandType::COMMIT;
    }
    else if (first_keyword == "rollback") {
        return CommandType::ROLLBACK;
    }
    
    return CommandType::UNKNOWN;
}

#endif