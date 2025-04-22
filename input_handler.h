#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <string>
#include <iostream>
using namespace std;

enum class CommandType{
    INSERT,
    SELECT,
    UNKOWN
};

CommandType parse_command(const string& input){
    if(input.find("insert") == 0){
        return CommandType::INSERT;
    }
    else if(input == "select"){
        return CommandType::SELECT;
    }
    else{
        return CommandType::UNKOWN;
    }
}

#endif