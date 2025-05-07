#ifndef BTREE_H
#define BTREE_H

#include <iostream>
#include <vector>
#include <algorithm>
#include "row.h"

using namespace std;

const int ORDER = 4; // Max keys per node

// B-Tree Node Structure
struct BTreeNode {
    bool is_leaf;
    vector<Row> rows;  // Store full Row objects
    vector<BTreeNode*> children;

    BTreeNode(bool leaf) : is_leaf(leaf) {}
};

// B-Tree Class
class BTree {
public:
    BTreeNode* root;
    BTree() { root = new BTreeNode(true); }

    void insert(int id, string name, string email); // Fixed declaration
    void split_child(BTreeNode* parent, int index);
    void traverse(BTreeNode* node);
};



void BTree::insert(int id, string name, string email) {
    Row row = create_row(id, name, email);
    BTreeNode* current = root;

    // Check if the ID already exists in the B-Tree
    while (!current->is_leaf) {
        int i = 0;
        while (i < current->rows.size() && id > current->rows[i].id) i++;
        current = current->children[i];
    }

    //  Verify uniqueness before inserting
    for (const Row& existing_row : current->rows) {
        if (existing_row.id == id) { 
            cout << "Error: Duplicate primary key (ID " << id << ") detected. Insert failed.\n";
            return;  
        }
    }

    //// If no duplicate exists, insert the row
    current->rows.push_back(row);
    sort(current->rows.begin(), current->rows.end(), [](Row a, Row b) {
        return a.id < b.id;
    });

    cout << "Row inserted in B-Tree.\n"; 
}


// Splitting Overloaded Nodes
void BTree::split_child(BTreeNode* parent, int index) {
    BTreeNode* child = parent->children[index];
    BTreeNode* new_child = new BTreeNode(child->is_leaf);

    int mid = ORDER / 2;
    parent->rows.insert(parent->rows.begin() + index, child->rows[mid]); // Fix
    new_child->rows.assign(child->rows.begin() + mid + 1, child->rows.end());
    child->rows.resize(mid);

    if (!child->is_leaf) {
        new_child->children.assign(child->children.begin() + mid + 1, child->children.end());
        child->children.resize(mid + 1);
    }

    parent->children.insert(parent->children.begin() + index + 1, new_child);
}

// Traversing the B-Tree
void BTree::traverse(BTreeNode* node) {
    if (!node) return;
    for (int i = 0; i < node->rows.size(); i++) {
        if (!node->is_leaf) traverse(node->children[i]);
        print_row(node->rows[i]); // Print full row data
    }
    if (!node->is_leaf) traverse(node->children[node->rows.size()]);
}

#endif
