//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_HANDLER_H
#define PEERNODE_HANDLER_H

#include <array>
#include <set>
#include <string>

#include "BlockChain.h"
#include "Formatter.h"

using namespace std;

class Handler {
private:
    string floodFile;
    vector<pair<string, int>> adversaries;
    BlockChain chain;
    const string selfIp;
    int selfPort;
    bool mStopped = false;

public:

    Handler(string ip, int port, long interArrivalTime, double nodeHashPower, string floodFile);

    ~Handler();

    pair<string, array<string, 3>> handle(int fd, const string &msg,
                                          const set<string> &peers,
                                          shared_mutex &mPeerListMutex);

    void broadcastToPeers(const set<string> &peers, const Block &blk,
                          shared_mutex &mPeerListMutex, bool sync = false);

    void startBroadcastTask(const set<string> &peers,
                            shared_mutex &mPeerListMutex);
    void flood();
    void init(bool wait);

    void startSyncTask(const set<string> &peers, shared_mutex &mPeerListMutex);
};

#endif    // PEERNODE_HANDLER_H
