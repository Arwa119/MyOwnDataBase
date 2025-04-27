#ifndef BTREE_H
#define BTREE_H
#include "row.h"

const int MaxKeys=4;
const int MaxChildren = MaxKeys + 1;


struct BtreeNode
{
    int keys[MaxKeys];
    Row rows[MaxKeys];
    BtreeNode *children[MaxChildren];
    bool isLeaf;
    int numKeys;

    //constructor
    BtreeNode(bool leaf);
};

#endif

class Btree {
    public:
        Btree();
        void insert(int key, const Row &row);       
         BtreeNode *search(int key);
         void loadFromFile(const string& filename);


    private:
        BtreeNode *root;
        void splitChild(BtreeNode *parent, int index, BtreeNode *child);
        void insertNonFull(BtreeNode *node, int key,const Row &row);
        BtreeNode *searchRecursive(BtreeNode *node, int key);
};