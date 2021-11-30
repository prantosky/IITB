//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_BLOCK_H
#define PEERNODE_BLOCK_H

#include <string>

#include "Helper.h"

using namespace std;

class Block {
private:
    int mHeight;
    string mPrevBlockHash;
    string mMerkleRoot;
    int mTimestamp;

public:
    static const size_t HASH_LENGTH = 16;
    static const size_t MERKLE_ROOT_LENGTH = 4;

    Block() = default;

    Block(string prevBlockHash, string merkelRoot, int timestamp);

    ~Block() = default;

    void setHeight(int height);

    int getHeight() const;

    string getMerkleRoot() const;

    int getTimeStamp() const;

    string getPreviousBlockHash() const;

    string getThisBlockHash() const;

    // Operator Overloading
    bool operator==(const Block &blk);

    friend ostream &operator<<(ostream &cout, const Block &blk);
};

#endif    // PEERNODE_BLOCK_H
