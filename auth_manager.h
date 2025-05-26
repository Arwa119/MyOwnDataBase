#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <string>

void loadUsers();  // Load users from file
bool registerUser(std::string username, std::string password, std::string role); // Register new user
std::string authenticateUser(std::string username, std::string password); // Validate login

#endif
