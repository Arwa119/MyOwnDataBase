#include <iostream>
#include <string>
#include <sstream>
#include "input_handler.h"
#include "row.h"
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
        {
            stringstream ss(input);
            string keyword;
            int id;
            string name, email;
            ss >> keyword >> id >> name >> email;
            if (ss.fail() || name.length() > COLUMN_NAME_SIZE || email.length() > COLUMN_EMAIL_SIZE)
            {
                cout << "Syntax error or field too long. Usage : insert <id> <name> <email>\n ";
                break;
            }

            Row row;
            row.id = id;
            strncpy(row.name, name.c_str(), COLUMN_NAME_SIZE);
            strncpy(row.email, email.c_str(), COLUMN_EMAIL_SIZE);

            cout << "Row Inserted: \n";
            print_row(row);
            break;
        }
        case CommandType::SELECT:
        {
            cout << "This is where will select.\n";
            break;
        }
        case CommandType::UNKOWN:
        default:
            cout << "Unrecognized command: " << input << "\n";
            break;
        }
    }
    return 0;
}