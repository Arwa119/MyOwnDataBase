#include "btree.h"
#include <iostream>
using namespace std;
#include <fstream>


//constructor for btree node
BtreeNode::BtreeNode(bool leaf):isLeaf(leaf),numKeys(0){}

//consttuctor for btree 
Btree::Btree():root(new BtreeNode(true)){}

//Search function 
BtreeNode* Btree::search(int key){
    return searchRecursive(root, key);
}

BtreeNode* Btree::searchRecursive(BtreeNode* node,int key ){
    int i = 0;
    while (i<node-> numKeys && key> node -> keys[i])
        i++;
    if(i<node->numKeys && node-> keys[i]==key){
        return node;
    }
    if(node->isLeaf){
        return nullptr;
    }
    return searchRecursive(node->children[i], key);
}

//insert function
void Btree::insert(int key, const Row &row) {
    if (root->numKeys == MaxKeys) {
        BtreeNode *newRoot = new BtreeNode(false);
        newRoot->children[0] = root;
        splitChild(newRoot, 0, root);
        root = newRoot;
        insertNonFull(root, key, row); 
    } else {
        insertNonFull(root, key, row); 
    }
}
//helper function to insert
void Btree::insertNonFull(BtreeNode* node, int key, const Row& row) {
    int i = node->numKeys - 1;

    if (node->isLeaf) {
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->rows[i + 1] = node->rows[i]; 
            i--;
        }
        node->keys[i + 1] = key;
        node->rows[i + 1] = row; 
        node->numKeys++;
    } else {
        while (i >= 0 && key < node->keys[i]) {
            i--;
        }
        i++;
        if (node->children[i]->numKeys == MaxKeys) {
            splitChild(node, i, node->children[i]);
            if (key > node->keys[i]) {
                i++;
            }
        }
        insertNonFull(node->children[i], key, row);
    }
}


//split child func splits childs when the tree is full 
void Btree ::splitChild(BtreeNode* parent ,int index,BtreeNode* child){
    BtreeNode* newChild = new BtreeNode(child->isLeaf); // New node for the second half
    newChild->numKeys = MaxKeys / 2;
    for (int j = 0; j < MaxKeys / 2; j++) {
        newChild->keys[j] = child->keys[j + MaxKeys / 2 + 1];
    }
    if (!child->isLeaf) {
        for (int j = 0; j < MaxKeys / 2 + 1; j++) {
            newChild->children[j] = child->children[j + MaxKeys/ 2 + 1];
        }
    }
    child->numKeys = MaxKeys / 2; 
    for (int j = parent->numKeys; j >= index + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[index + 1] = newChild;
    for (int j = parent->numKeys - 1; j >= index; j--) {
        parent->keys[j + 1] = parent->keys[j];
    }
    parent->keys[index] = child->keys[MaxKeys/ 2];
    parent->numKeys++;

}


// Function to load rows from the file into the B-Tree
void Btree::loadFromFile(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cout << "Error: Could not open file " << filename << " for reading.\n";
        return;
    }

    string line;
    while (getline(file, line)) {
        stringstream ss(line);
        int id;
        string name, email;
        char delimiter;
        ss >> id >> delimiter;
        getline(ss, name, ','); 
        getline(ss, email, ','); 
        Row row = create_row(id, name, email);

        insert(id, row); 
    }

    file.close();
    cout << "Data loaded into B-Tree from " << filename << ".\n";
}