#include "btree.h"
#include <iostream>
#include <algorithm>
#include <vector>
#include <limits>

BtreeNode::BtreeNode(bool leaf) : isLeaf(leaf), numKeys(0) {
    for (int i = 0; i < MaxChildren; ++i) {
        children[i] = nullptr;
    }
}

BtreeNode::~BtreeNode() {
}

int BtreeNode::findKeyIndex(int k) const {
    int index = 0;
    while (index < numKeys && keys[index] < k) {
        ++index;
    }
    return index;
}

Btree::Btree() : root(new BtreeNode(true)) {}

Btree::~Btree() {
    destroyTree(root);
}

void Btree::destroyTree(BtreeNode* node) {
    if (node == nullptr) {
        return;
    }
    if (!node->isLeaf) {
        for (int i = 0; i <= node->numKeys; ++i) {
            destroyTree(node->children[i]);
        }
    }
    delete node;
}

uint32_t Btree::search(int key) {
    return searchRecursive(root, key);
}

uint32_t Btree::searchRecursive(BtreeNode* node, int key) {
    int i = node->findKeyIndex(key);

    if (i < node->numKeys && node->keys[i] == key) {
        return node->row_nums[i];
    }

    if (node->isLeaf) {
        return std::numeric_limits<uint32_t>::max();
    }

    return searchRecursive(node->children[i], key);
}

void Btree::insert(int key, uint32_t row_num) {
    if (root->numKeys == MaxKeys) {
        BtreeNode* newRoot = new BtreeNode(false);
        newRoot->children[0] = root;
        splitChild(newRoot, 0, root);
        root = newRoot;
        insertNonFull(root, key, row_num);
    } else {
        insertNonFull(root, key, row_num);
    }
}

void Btree::insertNonFull(BtreeNode* node, int key, uint32_t row_num) {
    int i = node->numKeys - 1;

    if (node->isLeaf) {
        while (i >= 0 && key < node->keys[i]) {
            node->keys[i + 1] = node->keys[i];
            node->row_nums[i + 1] = node->row_nums[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->row_nums[i + 1] = row_num;
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
        insertNonFull(node->children[i], key, row_num);
    }
}

void Btree::splitChild(BtreeNode* parent, int index, BtreeNode* child) {
    BtreeNode* newChild = new BtreeNode(child->isLeaf);
    newChild->numKeys = MaxKeys / 2;

    for (int j = 0; j < MaxKeys / 2; j++) {
        newChild->keys[j] = child->keys[j + MaxKeys / 2 + 1];
        newChild->row_nums[j] = child->row_nums[j + MaxKeys / 2 + 1];
    }

    if (!child->isLeaf) {
        for (int j = 0; j < MaxKeys / 2 + 1; j++) {
            newChild->children[j] = child->children[j + MaxKeys / 2 + 1];
        }
    }

    child->numKeys = MaxKeys / 2;

    for (int j = parent->numKeys; j >= index + 1; j--) {
        parent->children[j + 1] = parent->children[j];
    }
    parent->children[index + 1] = newChild;

    for (int j = parent->numKeys - 1; j >= index; j--) {
        parent->keys[j + 1] = parent->keys[j];
        parent->row_nums[j + 1] = parent->row_nums[j];
    }
    parent->keys[index] = child->keys[MaxKeys / 2];
    parent->row_nums[index] = child->row_nums[MaxKeys / 2];

    parent->numKeys++;
}

BtreeNode* Btree::getRoot() const {
    return root;
}

void Btree::printTree(BtreeNode* node, int depth) const {
    if (node == nullptr) {
        return;
    }

    for (int i = 0; i < depth; ++i) std::cout << "  ";
    std::cout << "Node (Depth " << depth << ", Leaf: " << node->isLeaf << ", Keys: " << node->numKeys << "): ";
    for (int i = 0; i < node->numKeys; ++i) {
        std::cout << "[" << node->keys[i] << ":" << node->row_nums[i] << "] ";
    }
    std::cout << std::endl;

    if (!node->isLeaf) {
        for (int i = 0; i <= node->numKeys; ++i) {
            printTree(node->children[i], depth + 1);
        }
    }
}

bool Btree::remove(int key) {
    uint32_t row_num = search(key);
    if (row_num == std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    removeRecursive(root, key);

    if (root->numKeys == 0 && !root->isLeaf) {
        BtreeNode* oldRoot = root;
        root = root->children[0];
        delete oldRoot;
    }
    return true;
}

void Btree::removeRecursive(BtreeNode* node, int key) {
    int index = node->findKeyIndex(key);

    if (index < node->numKeys && node->keys[index] == key) {
        if (node->isLeaf) {
            removeFromLeaf(node, index);
        } else {
            removeFromInternal(node, index);
        }
    }
    else {
        if (node->isLeaf) {
            return;
        }

        BtreeNode* child = node->children[index];

        if (child->numKeys < MinKeys) {
            fill(node, index);
            index = node->findKeyIndex(key);
             child = node->children[index];

             if (index < node->numKeys && node->keys[index] == key) {
                 if (node->isLeaf) {
                     removeFromLeaf(node, index);
                 } else {
                     removeFromInternal(node, index);
                 }
                 return;
            }
        }

        removeRecursive(child, key);
    }
}

void Btree::removeFromLeaf(BtreeNode* node, int index) {
    for (int i = index + 1; i < node->numKeys; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->row_nums[i - 1] = node->row_nums[i];
    }
    node->numKeys--;
}

void Btree::removeFromInternal(BtreeNode* node, int index) {
    int keyToRemove = node->keys[index];

    if (node->children[index]->numKeys >= MinKeys) {
        int predecessor = getPredecessor(node->children[index], node->children[index]->numKeys);
        node->keys[index] = predecessor;
        removeRecursive(node->children[index], predecessor);
    }
    else if (node->children[index + 1]->numKeys >= MinKeys) {
        int successor = getSuccessor(node->children[index + 1], 0);
        node->keys[index] = successor;
        removeRecursive(node->children[index + 1], successor);
    }
    else {
        merge(node, index);
        removeRecursive(node->children[index], keyToRemove);
    }
}

int Btree::getPredecessor(BtreeNode* node, int index) {
    BtreeNode* currentNode = node->children[index];
    while (!currentNode->isLeaf) {
        currentNode = currentNode->children[currentNode->numKeys];
    }
    return currentNode->keys[currentNode->numKeys - 1];
}

int Btree::getSuccessor(BtreeNode* node, int index) {
    BtreeNode* currentNode = node->children[index];
    while (!currentNode->isLeaf) {
        currentNode = currentNode->children[0];
    }
    return currentNode->keys[0];
}

void Btree::fill(BtreeNode* node, int index) {
    if (index != 0 && node->children[index - 1]->numKeys >= MinKeys + 1) {
        borrowFromLeft(node, index);
    }
    else if (index != node->numKeys && node->children[index + 1]->numKeys >= MinKeys + 1) {
        borrowFromRight(node, index);
    }
    else {
        if (index != node->numKeys) {
            merge(node, index);
        } else {
            merge(node, index - 1);
        }
    }
}

void Btree::borrowFromRight(BtreeNode* node, int index) {
    BtreeNode* child = node->children[index];
    BtreeNode* rightSibling = node->children[index + 1];

    child->keys[child->numKeys] = node->keys[index];
    child->row_nums[child->numKeys] = node->row_nums[index];
    child->numKeys++;

    node->keys[index] = rightSibling->keys[0];
    node->row_nums[index] = rightSibling->row_nums[0];

    if (!child->isLeaf) {
        child->children[child->numKeys] = rightSibling->children[0];
        for (int i = 1; i <= rightSibling->numKeys; ++i) {
            rightSibling->children[i - 1] = rightSibling->children[i];
        }
    }

    for (int i = 1; i < rightSibling->numKeys; ++i) {
        rightSibling->keys[i - 1] = rightSibling->keys[i];
        rightSibling->row_nums[i - 1] = rightSibling->row_nums[i];
    }
    rightSibling->numKeys--;
}

void Btree::borrowFromLeft(BtreeNode* node, int index) {
    BtreeNode* child = node->children[index];
    BtreeNode* leftSibling = node->children[index - 1];

    for (int i = child->numKeys - 1; i >= 0; --i) {
        child->keys[i + 1] = child->keys[i];
        child->row_nums[i + 1] = child->row_nums[i];
    }

    if (!child->isLeaf) {
        for (int i = child->numKeys; i >= 0; --i) {
            child->children[i + 1] = child->children[i];
        }
        child->children[0] = leftSibling->children[leftSibling->numKeys];
    }

    child->keys[0] = node->keys[index - 1];
    child->row_nums[0] = node->row_nums[index - 1];
    child->numKeys++;

    node->keys[index - 1] = leftSibling->keys[leftSibling->numKeys - 1];
    node->row_nums[index - 1] = leftSibling->row_nums[leftSibling->numKeys - 1];
    leftSibling->numKeys--;
}

void Btree::merge(BtreeNode* node, int index) {
    BtreeNode* child = node->children[index];
    BtreeNode* rightSibling = node->children[index + 1];

    child->keys[child->numKeys] = node->keys[index];
    child->row_nums[child->numKeys] = node->row_nums[index];
    child->numKeys++;

    for (int i = 0; i < rightSibling->numKeys; ++i) {
        child->keys[child->numKeys + i] = rightSibling->keys[i];
        child->row_nums[child->numKeys + i] = rightSibling->row_nums[i];
    }

    if (!child->isLeaf) {
        for (int i = 0; i <= rightSibling->numKeys; ++i) {
            child->children[child->numKeys + i] = rightSibling->children[i];
        }
    }

    child->numKeys += rightSibling->numKeys;

    for (int i = index + 1; i < node->numKeys; ++i) {
        node->keys[i - 1] = node->keys[i];
        node->row_nums[i - 1] = node->row_nums[i];
    }

    for (int i = index + 2; i <= node->numKeys; ++i) {
        node->children[i - 1] = node->children[i];
    }

    node->numKeys--;
    delete rightSibling;
}
