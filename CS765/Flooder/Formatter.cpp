//
// Created by Pranav on 09-11-2020.
//

#include "Formatter.h"

string Formatter::getDeadNodeMessage(const int timestamp, const string &ip,
                                     const int port, const string &selfIp) {
    return "Dead Node:" + ip + ":" + to_string(port) + ":" +
           to_string(timestamp) + ":" + ip;
}

string Formatter::getLivelinessRequestMessage(const string &selfIp,
                                              const int selfPort,
                                              const int timestamp) {
    // Liveness Request:<self.timestamp>:<self.IP>:<self.Port>
    return "Liveness Request:" + to_string(timestamp) + ":" + selfIp + ":" +
           to_string(selfPort);
}

string Formatter::getPeerListRequestMessage(const string &ip, const int port) {
    return "Peer List:" + ip + ":" + to_string(port) + ":0:Peer List";
}

string Formatter::getLivelinessReplyMessage(const string &senderIp,
                                            const int senderPort,
                                            const string &selfIp,
                                            const int selfPort,
                                            const string &timestamp) {
    // Liveness
    // Reply:<sender.timestamp>:<sender.IP>:<sender.port>:<self.IP>:<self.port>
    return "Liveness Reply:" + timestamp + ":" + senderIp + ":" +
           to_string(senderPort) + ":" + selfIp + ":" + to_string(selfPort);
}

string Formatter::getBlockString(const Block &blk) {
    return "Block:" + blk.getPreviousBlockHash() + ":" + blk.getMerkleRoot() +
           ":" + to_string(blk.getTimeStamp());
}

string Formatter::getSyncRequest(const Block &blk, const string &selfIp,
                                 const int selfPort) {
    return "Sync:" + blk.getPreviousBlockHash() + "-" + blk.getMerkleRoot() +
           "-" + to_string(blk.getTimeStamp()) + ":" + selfIp + ":" +
           to_string(selfPort);
}

array<string, 5> Formatter::parseLivelinessReply(const string &msg) {
    //  Liveness Reply:<sender.timestamp >:<sender.IP
    //  >:<sender.port>:<self.IP><self.port>
    array<string, 5> arr;
    if (msg.length() == 0) return arr;

    const regex re(R"(Liveness\sReply:(.+?):(.+?):(\d+):(.+?):(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        cerr << "Formatter::parseLivelinessReply: Parsing failed for string \""
             << msg << "\"" << endl;
        return arr;
    }
    if (match.size() != 6) {
        cerr << "Formatter::parseLivelinessReply: invalid string encountered "
                "while parsing \""
             << msg << "\"" << endl;
        return arr;
    }
    for (size_t i = 1; i <= 5; i++) {
        arr[i - 1] = match.str(i);
    }
    return arr;
}

bool Formatter::validateIP(const string &ip) {
    if (ip.length() == 0) return false;

    string octal = R"(([0-1]?\d{1,2}|2[0-4]\d|25[0-5]\.))";
    regex re(octal + R"(\.)" + octal + R"(\.)" + octal + R"(\.)" + octal);
    if (regex_match(ip.c_str(), re)) {
        return true;
    }
    cout << "Formatter::validateIP: IP validation failed for string \"" << ip
         << "\"" << endl;
    return false;
}

array<string, 2> Formatter::parseSeedsFromSeedFile(const string &str) {
    array<string, 2> arr;
    if (str.length() == 0) return arr;

    const regex re(R"((.+?):(\d+))");
    smatch match;
    if (not regex_search(str, match, re)) {
        cerr
                << "Formatter::parseSeedsFromSeedFile: Parsing failed for string \""
                << str << "\"" << endl;
        return arr;
    }
    if (match.size() != 3) {
        cerr << "Formatter::parseSeedsFromSeedFile: invalid string encountered "
                "while parsing \""
             << str << "\"" << endl;
        return arr;
    }
    for (size_t i = 1; i < 3; i++) {
        arr[i - 1] = match.str(i);
    }
    return arr;
}

vector<pair<string, int>> Formatter::parsePeerList(const string &msg) {
    string m = msg;
    vector<pair<string, int>> peers;
    const regex re(R"(Peer:(.+?):(\d+):Peer;?)");
    smatch match;

    while (m.length() > 0 and regex_search(m, match, re)) {
        if (validateIP(match.str(1))) {
            peers.emplace_back(match.str(1), stoi(match.str(2)));
        } else {
            cerr << "Formatter::parsePeerList: inavlid ip received as \""
                 << match.str(1) << "\"" << endl;
        }
        m = match.suffix();

        // cout << "Formatter::parsePeerList: string truncated " << m << endl;
    }
    cout << "Formatter::parsePeerList: " << peers.size() << " peers identified"
         << endl;
    return peers;
}

bool Formatter::validateLivelinessReply(const array<string, 5> &resp,
                                        const int timestamp,
                                        const string &peerIp,
                                        const int peerPort,
                                        const string &selfIp, int selfPort) {
    if (resp.size() != 5) {
        return false;
    }

    if (not Formatter::validateIP(resp[1]) or
        not Formatter::validateIP(resp[3])) {
        cerr << "PeerNode::_validateLivelinessReply: IP could not be validated"
             << endl;
        return false;
    }

    if (resp[1] == selfIp and resp[3] == peerIp and
        stoi(resp[2]) == selfPort and resp[0] == to_string(timestamp) and
        stoi(resp[4]) == peerPort) {
        return true;
    }
    return false;
}

bool Formatter::_parseLivelinessRequest(const string &msg,
                                        array<string, 3> &retval) {
    // Liveness Request:<self.timestamp>:<self.IP>:<self.Port>
    if (msg.length() == 0) return false;

    const regex re(R"(Liveness\sRequest:(.+?):(.+?):(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        return false;
    }
    if (match.size() != 4 and validateIP(match.str(2))) {
        return false;
    }
    for (size_t i = 1; i <= 3; i++) {
        retval[i - 1] = match.str(i);
    }
    return true;
}

bool Formatter::_parseBlock(const string &msg, array<string, 3> &retval) {
    // Block:<prevBlockHash>:<Merkel root>:<timestamp>
    if (msg.length() == 0) return false;

    const regex re(R"(Block:(.+?):(.+?):(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        return false;
    }
    if (match.size() != 4) {
        return false;
    }
    for (size_t i = 1; i <= 3; i++) {
        retval[i - 1] = match.str(i);
    }
    return true;
}

bool Formatter::_parseSyncBlock(const string &msg, array<string, 3> &retval) {
    // Sync_Block:<prevBlockHash>:<Merkel root>:<timestamp>
    if (msg.length() == 0) return false;

    const regex re(R"(Sync_Block:(.+?):(.+?):(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        return false;
    }
    if (match.size() != 4) {
        return false;
    }
    for (size_t i = 1; i <= 3; i++) {
        retval[i - 1] = match.str(i);
    }
    return true;
}

bool Formatter::_parseSyncRequest(const string &msg, array<string, 3> &retval) {
    // Sync:peerIp:peerPort:<prevBlockHash>-<Merkel root>-<timestamp>
    if (msg.length() == 0) return false;

    const regex re(R"(Sync:(.+?):(.+?):(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        return false;
    }
    if (match.size() != 4) {
        return false;
    }
    for (size_t i = 1; i <= 3; i++) {
        retval[i - 1] = match.str(i);
    }
    return true;
}

array<string, 4> Formatter::parse(const string &msg) {
    array<string, 4> arr{};
    array<string, 3> retval{};

    if (_parseLivelinessRequest(msg, retval)) {
        arr[0] = "Liveness_Request";
        for (size_t i = 0; i < retval.size(); i++) {
            arr[i + 1] = retval[i];
        }
    } else if (_parseSyncBlock(msg, retval)) {
        arr[0] = "Sync_Block";
        for (size_t i = 0; i < retval.size(); i++) {
            arr[i + 1] = retval[i];
        }
    } else if (_parseSyncRequest(msg, retval)) {
        arr[0] = "Sync";
        for (size_t i = 0; i < retval.size(); i++) {
            arr[i + 1] = retval[i];
        }
    } else if (_parseBlock(msg, retval)) {
        arr[0] = "Block";
        for (size_t i = 0; i < retval.size(); i++) {
            arr[i + 1] = retval[i];
        }
    } else {
        cerr << "Formatter::parse: Invalid string received \"" << msg << "\""
             << endl;
    }
    return arr;
}

optional<Block> Formatter::getBlockFromString(const string &msg) {
    if (msg.length() == 0) return nullopt;

    const regex re(R"((.+?)-(.+?)-(\d+))");
    smatch match;
    if (not regex_search(msg, match, re)) {
        return nullopt;
    }
    if (match.size() != 4) {
        return nullopt;
    }
    return Block(match.str(1), match.str(2), stoi(match.str(3)));
}

string Formatter::getSyncReply(const Block &blk) {
    return "Sync_Block:" + blk.getPreviousBlockHash() + ":" +
           blk.getMerkleRoot() + ":" + to_string(blk.getTimeStamp());
}
