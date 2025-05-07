#include <iostream>
#include <string>
#include <sstream>
#include "input_handler.h"
#include "btree.h" // Include B-Tree
#include "table.h"

using namespace std;

int main() {
    Table* table = db_open("mydb.db"); /// Open table with B-Tree

    string input;
    while (true) {
        cout << "db > ";
        getline(cin, input);

        if (input == ".exit") {
            db_close(table);
            cout << "Exiting...\n";
            break;
        }

        CommandType command = parse_command(input);
        switch (command) {
            case CommandType::INSERT: {
                stringstream ss(input);
                string keyword;
                int id;
                string name, email;
            
                ss >> keyword >> id >> name >> email;
                if (ss.fail()) {
                    cout << "Syntax error. Usage: insert <id> <name> <email>\n";
                    break;
                }
            
                table->btree->insert(id, name, email);  // Handles duplicate check inside
                break; // 
            }
            
            
        case CommandType::SELECT: {
            cout << "B-Tree Structure:\n";
            table->btree->traverse(table->btree->root); // Use B-Tree traversal
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
