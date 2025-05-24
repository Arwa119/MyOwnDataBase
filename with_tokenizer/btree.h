#ifndef BTREE_H
#define BTREE_H

#include <cstdint>
#include <iostream>
#include <limits>

const int MaxKeys = 4;
const int MaxChildren = MaxKeys + 1;
const int MinKeys = (MaxKeys + 1) / 2 - 1;


struct BtreeNode {
    int keys[MaxKeys];
    uint32_t row_nums[MaxKeys];
    BtreeNode *children[MaxChildren];
    bool isLeaf;
    int numKeys;

    BtreeNode(bool leaf);
    ~BtreeNode();

    int findKeyIndex(int k) const;
};

class Btree {
public:
    Btree();
    ~Btree();

    void insert(int key, uint32_t row_num);
    uint32_t search(int key);
    bool remove(int key);

    BtreeNode* getRoot() const;
    void printTree(BtreeNode* node, int depth = 0) const;

private:
    BtreeNode* root;

    uint32_t searchRecursive(BtreeNode* node, int key);
    void insertNonFull(BtreeNode* node, int key, uint32_t row_num);
    void splitChild(BtreeNode* parent, int index, BtreeNode* child);
    void destroyTree(BtreeNode* node);

    void removeRecursive(BtreeNode* node, int key);
    void removeFromLeaf(BtreeNode* node, int index);
    void removeFromInternal(BtreeNode* node, int index);
    int getPredecessor(BtreeNode* node, int index);
    int getSuccessor(BtreeNode* node, int index);
    void fill(BtreeNode* node, int index);
    void borrowFromRight(BtreeNode* node, int index);
    void borrowFromLeft(BtreeNode* node, int index);
    void merge(BtreeNode* node, int index);
};
#endif
