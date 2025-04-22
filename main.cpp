#include <iostream>
#include <string>
#include <sstream>
#include "input_handler.h"
#include "row.h"
#include "row_serialization.h"
#include "pager.h"
#include "table.h"

using namespace std;

int main() {
    Table* table = db_open("mydb.db");

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
            if (ss.fail() || name.length() > COLUMN_NAME_SIZE || email.length() > COLUMN_EMAIL_SIZE) {
                cout << "Syntax error or field too long. Usage: insert <id> <name> <email>\n";
                break;
            }

            if (table->num_rows >= TABLE_MAX_ROWS) {
                cout << "Error: Table full.\n";
                break;
            }

            Row row = create_row(id, name, email);
            void* row_slot = get_row_slot(table, table->num_rows);
            serialize_row(row, row_slot);
            table->num_rows++;

            cout << "Row inserted.\n";
            break;
        }
        case CommandType::SELECT: {
            for (uint32_t i = 0; i < table->num_rows; i++) {
                void* row_slot = get_row_slot(table, i);
                Row row;
                deserialize_row(row_slot, row);
                print_row(row);
            }
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
