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
    else{
        return CommandType::UNKOWN;
    }
}

#endif
