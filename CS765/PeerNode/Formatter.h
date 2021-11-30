//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_FORMATTER_H
#define PEERNODE_FORMATTER_H



#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "Block.h"

using namespace std;

class Formatter {
private:
    static bool _parseLivelinessRequest(const string& msg,
                                        array<string, 3>& retval);
    static bool _parseBlock(const string& blk, array<string, 3>& retval);
    static bool _parseSyncRequest(const string& blk, array<string, 3>& retval);
    static bool _parseSyncBlock(const string &msg, array<string, 3> &retval);

public:
    Formatter() = delete;
    ~Formatter() = delete;
    static bool validateIP(const string& ip);
    static bool validateLivelinessReply(
            const array<string, 5>& resp, int timestamp, const string& peerIp,
            int peerPort, const string& selfIp, int selfPort);

    // Message creating fucntion
    static string getPeerListRequestMessage(const string& ip, int port);
    static string getDeadNodeMessage(int timestamp, const string& ip,
                                     int port, const string& selfIp);
    static string getLivelinessRequestMessage(const string& selfIp,
                                              int selfPort,
                                              int timestamp);
    static string getLivelinessReplyMessage(const string& sender,
                                            int senderPort,
                                            const string& selfIp,
                                            int selfPort,
                                            const string& timestamp);
    static string getBlockString(const Block& blk);
    static string getSyncRequest(const Block& blk, const string& selfIp,
                                 int selfPort);
    static string getSyncReply(const Block& blk);

    // Parsing Funtions
    static array<string, 5> parseLivelinessReply(const string& msg);
    static vector<pair<string, int>> parsePeerList(const string& msg);
    static array<string, 2> parseSeedsFromSeedFile(const string& str);
    static array<string, 4> parse(
            const string& msg);	 // to be used from MessageHandler only

    static optional<Block> getBlockFromString(const string& msg);
};


#endif //PEERNODE_FORMATTER_H
