#include "auth_manager.h"
#include <fstream>
#include <unordered_map>
#include <iostream>

std::unordered_map<std::string, std::pair<std::string, std::string>> users; // username -> (password, role)

// Function to load users from file
void loadUsers() {
    std::ifstream file("users.txt");
    if (!file) {
        std::cerr << "Error: Could not open users.txt\n";
        return;
    }

    std::string username, password, role;
    while (file >> username >> password >> role) {
        users[username] = {password, role}; // Store in map
    }
    file.close();
}

// Function to register a new user and store in file
bool registerUser(std::string username, std::string password, std::string role) {
    if (users.find(username) != users.end()) {
        std::cerr << "Error: Username already exists.\n";
        return false;
    }

    users[username] = {password, role};

    std::ofstream file("users.txt", std::ios::app);
    if (!file) {
        std::cerr << "Error: Could not open users.txt\n";
        return false;
    }

    file << username << " " << password << " " << role << "\n";  // Append new user
    file.close();
    return true;
}

// Function to authenticate user login
std::string authenticateUser(std::string username, std::string password) {
    if (users.find(username) != users.end() && users[username].first == password) {
        return users[username].second;  // Return role if authentication succeeds
    }
    return "invalid";
}
