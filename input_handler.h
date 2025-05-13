#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>

enum class CommandType{
    INSERT,
    SELECT,
    DELETE,
    BTREE_CMD,
    CREATE_DB,
    USE_DB,
    CREATE_TABLE,
    ALTER_TABLE,
    SHOW_TABLES,
    DROP_TABLE,
    UNKOWN
};

CommandType parse_command(const std::string& input){
    auto first_char = std::find_if(input.begin(), input.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    });

    if (first_char == input.end()) {
        return CommandType::UNKOWN;
    }

    std::string trimmed_input(first_char, input.end());

    if(trimmed_input.find("insert") == 0){
        return CommandType::INSERT;
    }
    else if(trimmed_input.find("select") == 0){
        return CommandType::SELECT;
    }
    else if(trimmed_input.find("delete") == 0){
        return CommandType::DELETE;
    }
    else if(trimmed_input == ".btree"){
        return CommandType::BTREE_CMD;
    }
    ////db commands
    else if (trimmed_input .find("create database") == 0) {
        return CommandType::CREATE_DB;
    }
    else if (trimmed_input .find("use") == 0) {
        return CommandType::USE_DB;
    }
    else if (trimmed_input .find("create table") == 0) {
        return CommandType::CREATE_TABLE;
    }
    else if (trimmed_input .find("alter table") == 0) {
        return CommandType::ALTER_TABLE;
    }
    else if (trimmed_input .find("show tables") == 0) {
        return CommandType::SHOW_TABLES;
    }
    else if (trimmed_input.find("drop table") == 0) {
        return CommandType::DROP_TABLE;
    }
    else{
        return CommandType::UNKOWN;
    }
}

#endif
