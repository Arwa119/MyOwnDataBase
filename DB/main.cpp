#include <iostream>
#include <string>
#include "input_handler.h"
using namespace std;

// ------- REPL -----------
int main()
{
    string input;
    while (true)
    {
        cout << "db > ";
        getline(cin, input);
        if (input == ".exit")
        {
            cout << "Exiting...... ";
            break;
        }

        CommandType command = parse_command(input);
        switch (command)
        {
        case CommandType::INSERT:
            cout << "This is where will insert.\n";
            break;
        case CommandType::SELECT:
            cout << "This is where will select.\n";
            break;
        case CommandType::UNKOWN:
            cout << "Unrecognized command: " << input << "\n";
            break;
        }
    }
    return 0;
}