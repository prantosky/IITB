//
// Created by Pranav on 09-11-2020.
//

#include "Handler.h"

#include <utility>

Handler::Handler(const string ip, const int port, const long interArrivalTime,
				 const double nodeHashPower, string floodfile)
	: selfIp(ip),
	  selfPort(port),
	  chain(interArrivalTime, nodeHashPower),
	  floodFile(std::move(floodfile)) {}

pair<string, array<string, 3>> Handler::handle(int fd, const string &msg,
											   const set<string> &peers,
											   shared_mutex &mPeerListMutex) {
	auto arr = Formatter::parse(msg);

	if (arr.size() != 4) {
		return {};
	}
	if (arr.at(0) == "Liveness_Request") {
		Helper::sendDataToNode(
			fd, Formatter::getLivelinessReplyMessage(
					arr.at(2), stoi(arr.at(3)), selfIp, selfPort, arr.at(1)));
		return {"Liveness_Request", {arr.at(2), arr.at(3), arr.at(1)}};
	} else if (arr.at(0) == "Block") {
		auto blk = Formatter::getBlockFromString(arr.at(1) + "-" + arr.at(2) +
												 "-" + arr.at(3));
		if (blk.has_value()) {
			cout << "Block: " << blk.value() << endl;
			if (chain.addToQueue(blk.value())) {
				chain.pushBroadcastBlock(blk.value());
			}
		}

	} else if (arr.at(0) == "Sync") {
		// Serving the sync request
		auto blk = Formatter::getBlockFromString(arr.at(1));
		if (blk.has_value()) {
			auto prevBlock = chain.checkForPrevious(blk.value());
			cout << " Handler::handle: serving sync request: " << blk.value()
				 << endl;
			//            cout << "Response: " << prevBlock.value() << endl;

			if (prevBlock.has_value()) {
				int reqFd =
					Helper::connectToNode(arr.at(2), stoi(arr.at(3)), true);
				Helper::sendDataToNode(
					reqFd, Formatter::getSyncReply(prevBlock.value()));
			} else {
				cerr << "Handler::Handler: No previous block found for "
					 << blk.value() << endl;
			}
		}
	} else if (arr.at(0) == "Sync_Block") {
		// Process the requested Sync block here
		auto blk = Formatter::getBlockFromString(arr.at(1) + "-" + arr.at(2) +
												 "-" + arr.at(3));
		if (blk.has_value()) {
			cout << "Sync_Block: " << blk.value() << endl;
			chain.addToQueue(blk.value());
		}
	} else {
		cout << "Handler::handle: Invalid request received \"" << msg << "\""
			 << endl;
	}
	cout << endl;
	return {};
}

void Handler::broadcastToPeers(const set<string> &peers, const Block &blk,
							   shared_mutex &mPeerListMutex, bool sync) {
	shared_lock lck(mPeerListMutex);
	for (auto &peer : peers) {
		int i = peer.find(':');
		string ip = peer.substr(0, i);
		int port = stoi(peer.substr(i + 1));
		int fd = Helper::connectToNode(ip, port, true);
		if (sync) {
			Helper::sendDataToNode(
				fd, Formatter::getSyncRequest(blk, selfIp, selfPort));
		} else {
			Helper::sendDataToNode(fd, Formatter::getBlockString(blk));
		}
		close(fd);
	}
}

// Looks for blocks to be broadcasted.
// TO be called from a separate thread always.
void Handler::startBroadcastTask(const set<string> &peers,
								 shared_mutex &mPeerListMutex) {
	cout << "Handler::startBroadcastTask: Broadcast Thread Started" << endl;
	while (not mStopped) {
		auto blk = chain.popBroadcastBlock();
		{
			shared_lock lck(mPeerListMutex);
			//            cout << "Handler::startBroadcastTask: Broadcasting blk
			//            " << blk << endl;
			broadcastToPeers(peers, blk, mPeerListMutex);
		}
	}
}

void Handler::startSyncTask(const set<string> &peers,
							shared_mutex &mPeerListMutex) {
	cout << "Handler::startSyncTask: Sync Thread Started" << endl;
	while (not mStopped) {
		auto blk = chain.popSyncBlock();
		{
			cout << " Handler::startSyncTask: sending sync request: " << blk
				 << endl;
			shared_lock lck(mPeerListMutex);
			broadcastToPeers(peers, blk, mPeerListMutex, true);
		}
	}
}

void Handler::init(bool wait) {
	ifstream flood(floodFile, ifstream::in);
	if (not flood.is_open()) {
		cerr << "Handler::init: Error opening flood file" << endl;
		exit(EXIT_FAILURE);
	}
	string adversary;
	while (not flood.eof()) {
		flood >> adversary;
		if (adversary.length() != 0) {
			auto arr = Formatter::parseSeedsFromSeedFile(adversary);
			if (arr.size() == 2 and Formatter::validateIP(arr[0])) {
				adversaries.emplace_back(arr[0], stoi(arr[1]));
			}
		}
		adversary.clear();
	}
	for (auto &adv : adversaries) {
		cout << "Will flood: " << adv.first << adv.second << endl;
	}
	chain.init(selfIp, selfPort, wait);
	(new thread([this]() { this->flood(); }))->detach();
}

void Handler::flood() {
	while (not mStopped) {
		for (auto &adv : adversaries) {
			chain.pushBroadcastBlock(Block("0", "0", 0));
		}
		this_thread::sleep_for(chrono::milliseconds(200));
	}
}

Handler::~Handler() { mStopped = true; }
