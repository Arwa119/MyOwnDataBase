#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include "input_handler.h"
#include "row.h"
#include "btree.h"
#include <sstream>
using namespace std;

// ------- REPL -----------
int main()
{  //....
    Btree tree;
    tree.loadFromFile("data.txt");
    //............................
    
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
            //updated this insert command 
        case CommandType::INSERT: {
    stringstream ss(input);
    string keyword;
    int id;
    string name, email;
    ss >> keyword >> id >> name >> email;

    if (ss.fail() || name.length() > COLUMN_NAME_SIZE || email.length() > COLUMN_EMAIL_SIZE) {
        cout << "Syntax error or field too long. Usage: insert <id> <name> <email>\n";
        break;
    }
    Row row = create_row(id, name, email);

    tree.insert(id,row); 

    ofstream file("data.txt", ios::app); 
    if (file.is_open()) {
        file << id << "," << name << "," << email << "\n"; 
        file.close();
        cout << "Row Inserted and written to file.\n";
    } else {
        cout << "Error!!Could not open file for writing.\n";
    }

    print_row(row); 
    break;
}
//updated this select command
       case CommandType::SELECT: {
    stringstream ss(input); 
    string keyword;
    int id;
    ss >> keyword >> id;
    if (ss.fail()) {
        cout << "Syntax error. Usage: SELECT <id>\n";
        break;
    }
    BtreeNode* result = tree.search(id);
    if (result != nullptr) {
        cout << "Key " << id << " Found in the B-Tree!\n";
    } else {
        cout << "Key " << id << " Not found.\n";
    }
    break;
}
        case CommandType::UNKOWN:
        //updates search command 
        case CommandType::SEARCH: {
        stringstream ss(input);
        string keyword;
        int id;
        ss >> keyword >> id;
        if (ss.fail()) {
        cout << "Syntax error. Usage: SEARCH <id>\n";
        break;
      }
       BtreeNode* result = tree.search(id);
       if (result != nullptr) {
        cout << "Key " << id << " Found in the BTree!!!!\n";
       } else {
        cout << "Key " << id << " Not found :(\n";
      }
        break;
}
       
        default:
            cout << "Unrecognized command: " << input << "\n";
            break;
        }
        
    }
    return 0;
}