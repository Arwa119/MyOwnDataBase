import csv
from filelock import FileLock


TRANSACTION_STATES = {
    "INITIAL": "Initial",
    "BEGIN": "Begin",
    "ACTIVE": "Active",
    "COMMIT": "Commit",
    "ROLLBACK": "Rollback",
    "END": "End"
}

class TransactionManager:
    def __init__(self, file_path):
    
        self.file_path = file_path
        self.state = TRANSACTION_STATES["INITIAL"]
        self.temp_changes = []  # Store changes temporarily
        self.lock = FileLock(file_path + ".lock")  # File lock for isolation

    def begin(self):
       
        if self.state == TRANSACTION_STATES["INITIAL"]:
            self.state = TRANSACTION_STATES["BEGIN"]
            print("Transaction started.")
        else:
            print("Transaction already started.")

    def select(self, id=None):
       
        if self.state not in [TRANSACTION_STATES["BEGIN"], TRANSACTION_STATES["ACTIVE"]]:
            print("Transaction not active.")
            return None

        with open(self.file_path, mode='r', newline='') as file:
            reader = csv.DictReader(file)
            if id is not None:
                for row in reader:
                    if row['id'] == str(id):
                        return row
                return None
            return list(reader)

    def update(self, id, updates):
       
        if self.state not in [TRANSACTION_STATES["BEGIN"], TRANSACTION_STATES["ACTIVE"]]:
            print("Transaction not active.")
            return

        # Stage the update in temporary changes
        self.temp_changes.append(('update', id, updates))
        self.state = TRANSACTION_STATES["ACTIVE"]
        print(f"Update for id {id} staged.")

    def delete(self, id):
       
        if self.state not in [TRANSACTION_STATES["BEGIN"], TRANSACTION_STATES["ACTIVE"]]:
            print("Transaction not active.")
            return

        # Stage the delete in temporary changes
        self.temp_changes.append(('delete', id))
        self.state = TRANSACTION_STATES["ACTIVE"]
        print(f"Delete for id {id} staged.")

    def commit(self):
       
        if self.state != TRANSACTION_STATES["ACTIVE"]:
            print("No active changes to commit.")
            return

       
        with self.lock:
            rows = []
            # Read the current data from the CSV file
            with open(self.file_path, mode='r', newline='') as file:
                reader = csv.DictReader(file)
                rows = list(reader)

            # Apply all temporary changes
            for change in self.temp_changes:
                operation, id, *args = change
                if operation == 'update':
                    updates = args[0]
                    for row in rows:
                        if row['id'] == str(id):
                            row.update(updates)
                            break
                elif operation == 'delete':
                    rows = [row for row in rows if row['id'] != str(id)]

            # Write the updated data back to the CSV file
            with open(self.file_path, mode='w', newline='') as file:
                writer = csv.DictWriter(file, fieldnames=reader.fieldnames)
                writer.writeheader()
                writer.writerows(rows)

            # Clear temporary changes and update state
            self.temp_changes = []
            self.state = TRANSACTION_STATES["COMMIT"]
            print("Transaction committed successfully.")

    def rollback(self):

        if self.state != TRANSACTION_STATES["ACTIVE"]:
            print("No active changes to rollback.")
            return

        # Clear temporary changes and update state
        self.temp_changes = []
        self.state = TRANSACTION_STATES["ROLLBACK"]
        print("Transaction rolled back successfully.")

    def end(self):
     
        self.state = TRANSACTION_STATES["END"]
        print("Transaction ended.")


if __name__ == "__main__":
    
    transaction = TransactionManager('users.csv')
    transaction.begin()
    print("\nSelect all users:")
    print(transaction.select())
    print("\nUpdating user with id 1...")
    transaction.update(1, {'name': 'John Doe', 'age': '26'})
    print("\nDeleting user with id 3...")
    transaction.delete(3)
    print("\nCommitting transaction...")
    transaction.commit()
    transaction.end()



    