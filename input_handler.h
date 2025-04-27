#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <string>
#include <iostream>
using namespace std;

enum class CommandType{
    INSERT,
    SELECT,
    SEARCH,
    UNKOWN
};

CommandType parse_command(const string& input) {
    if (input.find("insert") == 0) {
        return CommandType::INSERT;
    }
    else if (input.find("select") == 0) { // added this for selct
        return CommandType::SELECT;
    }
     else if (input.find("search") == 0) { // added this for search
        return CommandType::SEARCH;
    }

    else {
        return CommandType::UNKOWN;
    }
}
#endif