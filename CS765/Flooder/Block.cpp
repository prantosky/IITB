//
// Created by Pranav on 09-11-2020.
//


#include "Block.h"


bool Block::operator==(const Block &blk) {
    return this->mMerkleRoot == blk.mMerkleRoot and
           this->mPrevBlockHash == blk.mPrevBlockHash and
           this->mTimestamp == blk.mTimestamp;
}

ostream &operator<<(ostream &out, const Block &blk) {
    out << blk.getPreviousBlockHash() << " : " << blk.getTimeStamp() << " : " << blk.getMerkleRoot() << " : "
        << blk.getThisBlockHash();
    return out;
}

int Block::getTimeStamp() const { return mTimestamp; }

string Block::getMerkleRoot() const { return mMerkleRoot; }

Block::Block(string prevBlockHash, string merkelRoot, int timestamp)
        : mPrevBlockHash(std::move(prevBlockHash)),
          mMerkleRoot(std::move(merkelRoot)),
          mTimestamp(timestamp),
          mHeight(-1) {
    if (mPrevBlockHash.length() > HASH_LENGTH) {
        mPrevBlockHash = mPrevBlockHash.substr(0, HASH_LENGTH);
    } else if (mPrevBlockHash.length() < HASH_LENGTH) {
        mPrevBlockHash =
                string(HASH_LENGTH - mPrevBlockHash.length(), '0') + mPrevBlockHash;
    }
    if (mMerkleRoot.length() > MERKLE_ROOT_LENGTH) {
        mMerkleRoot = mMerkleRoot.substr(0, MERKLE_ROOT_LENGTH);
    } else if (mMerkleRoot.length() < MERKLE_ROOT_LENGTH) {
        mMerkleRoot = string(MERKLE_ROOT_LENGTH - mMerkleRoot.length(), '0') + mMerkleRoot;
    }

    if (mTimestamp > numeric_limits<int>::max()) {
        mTimestamp = 0;
    }
}

void Block::setHeight(const int height) { mHeight = height; }

int Block::getHeight() const { return mHeight; }

string Block::getPreviousBlockHash() const { return mPrevBlockHash; }

string Block::getThisBlockHash() const {
    string timestamp = to_string(mTimestamp);
    if (timestamp.length() < 10) {
        timestamp = string(10 - timestamp.length(), '0') + timestamp;
    }
    return Helper::sha3_256(mPrevBlockHash + mMerkleRoot + timestamp);
}
