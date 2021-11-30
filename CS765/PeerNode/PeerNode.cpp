#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <set>
#include <shared_mutex>
#include <utility>

#include "Handler.h"

using namespace std;

class PeerNode {
   private:
	const int BACK_LOG = 5;
	const int MAX_BUFFER_SIZE = 4096;
	const int MAX_EVENTS = 20;
	const size_t mPort;
	const string mIp;
	const string mSeedFile;
	shared_mutex mPeerListMutex;
	int epoll_fd{};
	int peerFd{};
	bool mTerminateFlag = false;

	Handler handler;

	vector<pair<string, int>> mSeeds;
	set<string> mNeighbors;

	// private functions
	void _sendDeadMsgToSeeds(const string &ip, int peerPort);

	void _removePeer(const string &ip, int peerPort);

	void _readSeedsFromFile();

	vector<pair<string, int>> _getPeersFromSeeds(
		vector<pair<string, int>> &seeds);

	void _handlePeerLiveliness(int fd, const string &peerIp, int peerPort);

	void _processOutput(const pair<string, array<string, 3>> &output);

   public:
	PeerNode(string ip, const size_t port, string seedfile,
			 const long interArrivalTime, const double nodeHashPower)
		: mIp(ip),
		  mPort(port),
		  mSeedFile(std::move(seedfile)),
		  handler(ip, port, interArrivalTime, nodeHashPower) {}

	~PeerNode() { mTerminateFlag = true; };

	void init();

	void start();
};

void PeerNode::_removePeer(const string &peerIp, const int peerPort) {
	unique_lock w_lock(mPeerListMutex);
	auto it = mNeighbors.find(peerIp + ":" + to_string(peerPort));
	if (it == mNeighbors.end()) {
		cerr << "PeerNode::removePeer: No socket found for ip " << peerIp
			 << " on port " << peerPort << endl;
	} else {
		mNeighbors.erase(it);
	}
}

void PeerNode::_sendDeadMsgToSeeds(const string &ip, const int port) {
	int timestamp = Helper::getSystemTimeStamp();
	string msg = Formatter::getDeadNodeMessage(timestamp, ip, port, ip);
	int fd = -1;
	for (auto &&i : mSeeds) {
		fd = Helper::connectToNode(i.first, i.second, true);
		if (fd != -1) {
			Helper::sendDataToNode(fd, msg);
		}
		close(fd);
	}
}

void PeerNode::_handlePeerLiveliness(const int fd, const string &peerIp,
									 const int peerPort) {
	int dead = 0;
	int timestamp;
	char buffer[MAX_BUFFER_SIZE];
	string response;
	chrono::_V2::steady_clock::time_point start, end;
	Helper::setTimeoutForSocket(fd);

	while (dead < 3) {
		//        cout << "dead: " << dead << endl;
		memset(&buffer, 0, sizeof(buffer));
		timestamp = Helper::getSystemTimeStamp();
		string msg = Formatter::getLivelinessRequestMessage(
			this->mIp, this->mPort, timestamp);
		//        cout << "Request: " << msg << endl;
		if (not Helper::sendDataToNode(fd, msg)) {
			//            cerr << "PeerNode::handlePeerLiveliness: Could not
			//            write data to "
			//                 << peerIp << ":" << peerPort << endl;
			dead++;
			start = chrono::steady_clock::now();
		} else {
			start = chrono::steady_clock::now();
			read(fd, buffer, MAX_BUFFER_SIZE);
			response = string(buffer);

			//            cout << "Response: " << response << endl;
			auto resp = Formatter::parseLivelinessReply(response);

			if (Formatter::validateLivelinessReply(resp, timestamp, peerIp,
												   peerPort, mIp, mPort)) {
				dead = 0;
			} else {
				//                cout << "PeerNode::handlePeerLiveliness: No
				//                reply from "
				//                     << peerIp << ":" << peerPort << " on " <<
				//                     mIp << ":"
				//                     << mPort << endl;
				dead++;
			}
		}
		do {
			this_thread::sleep_for(chrono::milliseconds(1000));
			end = chrono::steady_clock::now();
			//            cout << "Waiting: "
			//                 << chrono::duration_cast<chrono::seconds>(end -
			//                 start).count()
			//                 << endl;
		} while (chrono::duration_cast<chrono::seconds>(end - start).count() <
				 13);
	}
	_removePeer(peerIp, peerPort);
	_sendDeadMsgToSeeds(peerIp, peerPort);
	close(fd);
}

void PeerNode::_readSeedsFromFile() {
	vector<pair<string, int>> seeds;
	ifstream seedfile(mSeedFile, ifstream::in);
	if (not seedfile.is_open()) {
		cerr << "PeerNode::readSeedsFromFile: Error opening seed file" << endl;
		exit(EXIT_FAILURE);
	}
	string seed;
	while (not seedfile.eof()) {
		seedfile >> seed;
		if (seed.length() != 0) {
			auto arr = Formatter::parseSeedsFromSeedFile(seed);
			if (arr.size() == 2 and Formatter::validateIP(arr[0])) {
				seeds.emplace_back(arr[0], stoi(arr[1]));
			}
		}
		seed.clear();
	}
	// Selecting only n/2 seeds out of n randomly.
	size_t n = floor(seeds.size() / 2) + 1;
	shuffle(begin(seeds), end(seeds), default_random_engine{});
	while (seeds.size() > n) {
		seeds.pop_back();
	}
	seedfile.close();
	this->mSeeds = seeds;
}

vector<pair<string, int>> PeerNode::_getPeersFromSeeds(
	vector<pair<string, int>> &seeds) {
	vector<pair<string, int>> peers;
	string msg = Formatter::getPeerListRequestMessage(this->mIp, this->mPort);
	string retval;
	for (auto &&i : seeds) {
		retval = Helper::sendAndGetData(i.first, i.second, msg);
		auto p = Formatter::parsePeerList(retval);
		if (not p.empty()) {
			peers.insert(peers.end(), p.begin(), p.end());
		}
	}
	if (not peers.empty()) {
		shuffle(begin(peers), end(peers), default_random_engine{});
		while (peers.size() > 4) {
			peers.pop_back();
		}
	}
	return peers;
}

void PeerNode::init() {
	struct sockaddr_in addr {};
	peerFd = socket(AF_INET, SOCK_STREAM, 0);
	if (peerFd == -1) {
		cerr << "Error getting a socket" << endl;
		exit(EXIT_FAILURE);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(mPort);
	addr.sin_addr.s_addr = inet_addr(mIp.c_str());
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(peerFd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		cout << "Binding successful" << endl;
	} else {
		cerr << "Binding failed" << endl;
		exit(EXIT_FAILURE);
	}
	if (listen(peerFd, BACK_LOG) == 0) {
		// cout << "Listening on port: " << port << endl;
	} else {
		cerr << "Listening failed" << endl;
		exit(EXIT_FAILURE);
	}
	/* set O_NONBLOCK on fd */
	long flags = fcntl(peerFd, F_GETFL, 0);
	fcntl(peerFd, F_SETFL, flags | O_NONBLOCK);

	_readSeedsFromFile();
	for (auto &&i : mSeeds) {
		cout << "Seed: " << i.first << ":" << i.second << endl;
	}

	auto neighbors = _getPeersFromSeeds(mSeeds);

	for (auto &&i : neighbors) {
		cout << "Peer: " << i.first << ":" << i.second << endl;
	}

	for (auto &&i : neighbors) {
		int fdForLivelinessMsgs = Helper::connectToNode(i.first, i.second);
		if (fdForLivelinessMsgs == -1) {
			cerr << "Error connecting to peer " << i.first << ":" << i.second
				 << " from " << mIp << ":" << mPort << endl;
			continue;
		}

		cout << "Peer: " << i.first << ":" << i.second << endl;
		{
			unique_lock w_lock(mPeerListMutex);
			mNeighbors.insert(i.first + ":" + to_string(i.second));
		}
		(new thread([this, i, fdForLivelinessMsgs]() {
			cout << "PeerNode::init: Launching thread for liveliness of "
				 << i.first << ":" << i.second << endl;
			_handlePeerLiveliness(fdForLivelinessMsgs, i.first, i.second);
		}))->detach();
	}

	handler.init(not mNeighbors.empty());

	// Start a thread that broadcasts blocks
	(new thread([this]() {
		this->handler.startBroadcastTask(this->mNeighbors,
										 this->mPeerListMutex);
	}))->detach();

	// Start the thread that takes care of the out of order blocks
	(new thread([this]() {
		this->handler.startSyncTask(this->mNeighbors, this->mPeerListMutex);
	}))->detach();
}

void PeerNode::_processOutput(const pair<string, array<string, 3>> &output) {
	if (output.first == "Liveness_Request" and output.second.size() == 3) {
		string s = output.second[0] + ":" + output.second[1];
		auto it = mNeighbors.find(s);
		if (it == mNeighbors.end()) {
			int fdForLivelinessMsgs =
				Helper::connectToNode(output.second[0], stoi(output.second[1]));
			if (fdForLivelinessMsgs == -1) {
				cerr << "PeerNode::_processOutput::Error connecting to peer "
					 << output.second[0] << ":" << output.second[1] << " from "
					 << mIp << ":" << mPort << endl;
			} else {
				(new thread([this, fdForLivelinessMsgs, output]() {
					_handlePeerLiveliness(fdForLivelinessMsgs, output.second[0],
										  stoi(output.second[1]));
				}))->detach();
				cout << "New Peer connected with IP " << output.second[0]
					 << " and port " << output.second[1] << endl;
				{
					unique_lock w_lock(mPeerListMutex);
					mNeighbors.insert(s);
				}
			}
		}
	}
}

void PeerNode::start() {
	struct sockaddr client {};
	int addrlen = 0;
	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		cerr << "Error creating epoll instance" << endl;
		exit(EXIT_FAILURE);
	}
	struct epoll_event events[MAX_EVENTS];
	int sockfd;
	int nfds = 0;
	char buffer[MAX_BUFFER_SIZE];
	int n = 0;
	set<int> doneNfds;

	while (true) {
		memset(&client, 0, sizeof(client));
		addrlen = sizeof(client);
		sockfd =
			accept(peerFd, (struct sockaddr *)&client, (socklen_t *)&addrlen);
		if (sockfd != -1) {
			struct epoll_event ev {};
			ev.events = EPOLLIN | EPOLLET;
			ev.data.fd = sockfd;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
				cerr << "Failed to add file descriptor to epoll\n";
				close(epoll_fd);
				exit(EXIT_FAILURE);
			}
		}
		do {
			nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
		} while (errno == EINTR);
		int i = 0;
		while (i < nfds) {
			memset(buffer, 0, MAX_BUFFER_SIZE);
			n = read(events[i].data.fd, buffer, MAX_BUFFER_SIZE);
			string s = string(buffer);
			if (s.length() > 0) {
				// vector<pair<string, int>> adjacents;
				// copy(neighbors.begin(), neighbors.end(), adjacents.begin());
				auto output = handler.handle(events[i].data.fd, s, mNeighbors,
											 mPeerListMutex);
				_processOutput(output);
				i++;
			} else {
				nfds--;
			}
		}
	}
};

void printUsage(string s) {
	cout << "Usage: " << s
		 << " -ip=XXX.XXX.XXX.XXX -port=XXXX -seedfile=XXX "
			"-interArrivalTime=XX -nodeHashPower=XX"
		 << endl;
	exit(EXIT_FAILURE);
}

int main(int argc, char const *argv[]) {
	if (argc != 6) {
		printUsage(string(argv[0]));
	}

	int port;
	string seedfile;
	string ip;
	time_t interArrivalTime;
	double nodeHashPower;

	string arg;
	size_t pos = 0;
	for (int i = 1; i < argc; i++) {
		arg = string(argv[i]);
		if (i == 1 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "ip") {
				ip = arg.substr(pos + 1, arg.length());
				cout << "IP: " << ip << endl;
			} else {
				printUsage(string(argv[0]));
			}
		} else if (i == 2 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "port") {
				port = stoi(arg.substr(pos + 1, arg.length()));
				cout << "Port: " << port << endl;
			} else {
				printUsage(string(argv[0]));
			}
		} else if (i == 3 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "seedfile") {
				seedfile = arg.substr(pos + 1, arg.length());
				cout << "seedfile: " << seedfile << endl;
			} else {
				printUsage(string(argv[0]));
			}
		} else if (i == 4 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "interArrivalTime") {
				interArrivalTime = stoi(arg.substr(pos + 1, arg.length()));
				cout << "interArrivalTime: " << interArrivalTime << endl;
			} else {
				printUsage(string(argv[0]));
			}
		} else if (i == 5 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "nodeHashPower") {
				nodeHashPower = stod(arg.substr(pos + 1, arg.length()));
				cout << "nodeHashPower: " << nodeHashPower << endl;
			} else {
				printUsage(string(argv[0]));
			}
		} else {
			printUsage(string(argv[0]));
		}
	}
	if (Formatter::validateIP(ip)) {
		PeerNode node(ip, port, seedfile, interArrivalTime, nodeHashPower);
		node.init();
		node.start();
	} else {
		cout << "Invalid IP" << endl;
	}
	return 0;
}
