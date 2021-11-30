//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_BLOCKCHAIN_H
#define PEERNODE_BLOCKCHAIN_H

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <iomanip>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <thread>
#include <unordered_map>
#include <fstream>

#include "DBConnector.h"

using namespace std;

class BlockChain {
private:
    ofstream selfBlocksGenerated;

    const string mGenesisBlockHash = "0000000000009e1c";
    unordered_map<string, Block> endBlocks;
    unordered_map<string, Block> cache;
    unordered_map<string, pair<Block, int>> mWaitingBlocks;

    mt19937 rndGen;
    exponential_distribution<double> mDistribution;
    DBConnector connection{};

    mutex mSyncBlockQueueMutex;
    condition_variable mSyncBlockCV;
    queue<Block> mSyncBlockQueue;

    mutex mBroadcastQueueMutex;
    queue<Block> mBroadcastQueue;
    condition_variable mBroadcastCV;

    mutex mWaitingBlocksQueueMutex;
    condition_variable cv;
    bool stopped = false;

    mutex endPointMutex;

    string mMiningOnBlock;
    mutex mResetMutex;
    long mInterarrivalTime;
    double mNodeHashPower;

    string getMinigOnBlock();
    string getMinigOnBlockUpdated();
    void setMiningOnBlock(const string &newBlk);

    static bool _validateBlock(const Block &blk);

    void _updateEndBlocks(Block &blk);

    double _getRandomExpNumber();

    bool _addBlock(Block &blk);

    void _keepCheckingBlocksForInsertion();

    inline void _removeFromWaitingQueue(const Block &blk);

    optional<Block> _getMaxHeightBlock();
    bool _compareWithMaxHeightBlock(const string& hash,Block& retBlk);

    void _createBlock(bool wait);

    void pushSyncBlock(const Block &blk);

public:
    BlockChain();

    BlockChain(long interarrival_Time, double nodeHashPower);

    ~BlockChain();

    bool addToQueue(const Block &blk);

    optional<Block> checkForPrevious(Block &blk);

    Block popSyncBlock();

    Block popBroadcastBlock();

    void pushBroadcastBlock(const Block &blk);

    void init(const string& ip,int port, bool wait);
};

#endif    // PEERNODE_BLOCKCHAIN_H
