#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Formatter.cpp"

using namespace std;

class SeedNode {
   private:
	// constants
	const int BACK_LOG = 5;
	const int MAX_BUFF = 1024;
	const int MAX_EVENTS = 20;

	// members
	vector<pair<string, int>> peers;
	int serverFd{};
	const string ip;
	const size_t port;
	int epoll_fd{};
	ofstream logFile;

	// member functions
	void printPeers();
	bool addPeer(const string& ip, const int peerPort);
	void removePeer(const string& ip, const int peerPort);
	inline void createServerSocket();
	void handle(const int fd, const string& s);
	void execute();
	void sendPeerList(const int fd);

   public:
	SeedNode(const string& ip, const size_t port) : ip(ip), port(port) {
		logFile.open("log.txt", ofstream::out);
		if (not logFile.is_open()) {
			cerr << "SeedNode::Seednode: Log file creation failed" << endl;
		}
	}

	SeedNode(const int port) : ip("127.0.0.1"), port(port) {
		logFile.open("log.txt", ofstream::out);
		if (not logFile.is_open()) {
			cerr << "SeedNode::SeedNode: Log file creation failed" << endl;
		}
	}

	~SeedNode() {
		close(serverFd);
		close(epoll_fd);
		logFile.close();
	}

	void start();
};

void SeedNode::printPeers() {
	cout << "************************************" << endl;
	for (auto&& i : peers) {
		cout << "Peer: " << i.first << ":" << i.second << endl;
	}
}

void SeedNode::createServerSocket() {
	struct sockaddr_in addr {};
	serverFd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverFd == -1) {
		cerr << "SeedNode::createServerSocket: Error getting a socket" << endl;
		exit(EXIT_FAILURE);
	} else {
		cout << "SeedNode::createServerSocket: socket created" << endl;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		cerr << "SeedNode::createServerSocket: Binding failed" << endl;
		exit(EXIT_FAILURE);
	} else {
		cout << "SeedNode::createServerSocket: Binding Successful" << endl;
	}
	if (listen(serverFd, BACK_LOG) != 0) {
		cerr << "SeedNode::createServerSocket: Listening failed" << endl;
	} else {
		cout << "SeedNode::createServerSocket: listening..." << endl;
	}
	/* set O_NONBLOCK on fd */
	long flags = fcntl(serverFd, F_GETFL, 0);
	fcntl(serverFd, F_SETFL, flags | O_NONBLOCK);
}

bool SeedNode::addPeer(const string& ip, const int peerPort) {
	logFile << "Connection request received from new node " << ip << ":"
			<< peerPort << endl;
	cout << "SeedNode::addPeer: Connection request received from new node "
		 << ip << ":" << peerPort << endl;
	if (Formatter::validateIP(ip)) {
		for (auto&& i : peers) {
			if (i.first == ip and i.second == peerPort) {
				cerr << "SeedNode::addPeer: Socket already open for ip " << ip
					 << " on port " << peerPort << endl;
				return false;
			}
		}
		peers.push_back(make_pair(ip, peerPort));
	} else {
		return false;
	}
	return true;
}

void SeedNode::removePeer(const string& ip, const int peerPort) {
	logFile << "Connection request received for dead node " << ip << ":"
			<< peerPort << endl;
	cout << "SeedNode::removePeer: Connection request received for dead node "
		 << ip << ":" << peerPort << endl;
	for (auto it = peers.begin(); it != peers.end(); it++) {
		if (it->first == ip and it->second == peerPort) {
			peers.erase(it);
			return;
		}
	}
	cerr << "SeedNode::removePeer: No socket found for ip " << ip << " on port "
		 << peerPort << endl;
}

void SeedNode::handle(const int fd, const string& s) {
	cout << "SeedNode::handle: Received: " << s << endl;
	auto tokens = Formatter::parse(s);
	for (auto& i : tokens) {
		cout << "token: " << i << endl;
	}
	cout << endl;

	if (tokens.size() == 5 and tokens[0] == "Dead Node") {
		removePeer(tokens[1], stoi(tokens[2]));
	} else if (tokens.size() == 5 and tokens[0] == "Peer List") {
		sendPeerList(fd);
		addPeer(tokens[1], stoi(tokens[2]));
	} else {
		write(fd, "", 0);
	}
}

void SeedNode::sendPeerList(const int fd) {
	string msg = "";
	for (size_t i = 0; i < peers.size(); i++) {
		msg += "Peer:" + peers[i].first + ":" + to_string(peers[i].second) +
			   ":Peer;";
	}
	if (msg.length() > 0) msg.pop_back();
	cout << "SeedNode::sendPeerList: Writing on fd: " << fd << endl;
	cout << "SeedNode::sendPeerList: writing msg: \"" << msg << "\""
		 << " of length: " << msg.length() << endl;
	if (msg.length() == 0) msg = "NULL";
	ssize_t n = send(fd, msg.c_str(), msg.length(), 0);
	if (n > 0 and msg.length() == n) {
		cout << "SeedNode::sendPeerList: Peer list sent to the requesting peer"
			 << endl;
	} else {
		cout << "SeedNode::sendPeerList: Peer list sending failed to the "
				"requesting peer. Length written: "
			 << n << endl;
	}
}

void SeedNode::start() {
	createServerSocket();
	struct sockaddr client {};
	int addrlen = 0;
	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		cerr << "SeedNode::start: Error creating epoll instance" << endl;
		exit(EXIT_FAILURE);
	} else {
		cout << "SeedNode::createServerSocket: epoll instance created" << endl;
	}

	struct epoll_event events[MAX_EVENTS];
	int clientSocketFd = -1;
	int nfds = 0;
	char buffer[MAX_BUFF];
	unordered_set<int> doneNfds;

	new thread([this]() {
		while (true) {
			printPeers();
			this_thread::sleep_for(chrono::seconds(10));
		}
	});

	while (true) {
		memset(&client, 0, sizeof(client));
		addrlen = sizeof(client);
		clientSocketFd =
			accept(serverFd, (struct sockaddr*)&client, (socklen_t*)&addrlen);

		if (clientSocketFd != -1) {
			cout << "SeedNode::start: Client connected on fd " << clientSocketFd
				 << endl;
			struct epoll_event ev {};
			ev.events = EPOLLIN | EPOLLET;
			ev.data.fd = clientSocketFd;
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev) == -1) {
				cerr << "SeedNode::start: Failed to add file descriptor to "
						"epoll for fd "
					 << clientSocketFd;
				close(epoll_fd);
				exit(EXIT_FAILURE);
			}
		}
		// Waiting for epoll event
		do {
			nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
		} while (errno == EINTR);
		// cout << "nfds: " << nfds << endl;
		int i = 0;
		// Serve all requests that are received
		while (i < nfds) {
			memset(buffer, 0, MAX_BUFF);
			read(events[i].data.fd, buffer, MAX_BUFF);
			string s = string(buffer);
			if (s.length() > 0) {
				handle(events[i].data.fd, s);
			} else {
				nfds--;
			}
			// close(events[i].data.fd);
			i++;
		}
	}
}

void printUsage(string s) {
	cout << "Usage: " << s << " -port=XXXX" << endl;
	exit(EXIT_SUCCESS);
}

int main(int argc, char const* argv[]) {
	cout << "argc: " << argc << endl;
	if (argc != 2) {
		printUsage(string(argv[0]));
	}

	int port;

	string arg;
	size_t pos = 0;
	for (int i = 1; i < argc; i++) {
		arg = string(argv[i]);
		if (i == 1 and (pos = arg.find('=')) != string::npos) {
			if (arg.substr(1, pos - 1) == "port") {
				port = stoi(arg.substr(pos + 1, arg.length()));
			} else {
				printUsage(string(argv[0]));
			}
		} else {
			printUsage(string(argv[0]));
		}
	}
	SeedNode node(port);
	node.start();

	return 0;
}
