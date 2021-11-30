//
// Created by Pranav on 09-11-2020.
//

#include "Helper.h"

void Helper::setTimeoutForSocket(int fd) {
	struct timeval tv {};
	tv.tv_sec = 13;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
}

string Helper::getRandomString(int length) {
	mt19937 generator{random_device{}()};
	uniform_int_distribution<int> distribution{'a', 'z'};
	string randString(length, '\0');
	for (auto &strChr : randString) strChr = distribution(generator);
	return randString;
}

int Helper::getSystemTimeStamp() {
	auto timestamp = chrono::duration_cast<std::chrono::seconds>(
						 chrono::system_clock::now().time_since_epoch())
						 .count();
	return timestamp;
}

bool Helper::sendDataToNode(const int fd, const string &msg) {
	if (fd <= 0) return false;
	int n = send(fd, msg.c_str(), msg.length(), MSG_NOSIGNAL);
	if (n <= 0 or static_cast<int>(msg.length()) != n) {
		return false;
	}
	return true;
}

int Helper::connectToNode(const string &ip, const int serverPort,
						  bool nonBlocking) {
	struct sockaddr_in addr {};
	int sockfd;

	/* Create socket and connect to server */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		cerr << "PeerNode::connectToServer: Network Error: could not create "
				"socket"
			 << endl;
		return sockfd;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(serverPort);
	addr.sin_addr.s_addr = inet_addr(ip.c_str());

	if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		cerr << "PeerNode::connectToServer: Network Error: Could not connect"
			 << endl;
		return -1;
	}
	if (nonBlocking) {
		long flags = fcntl(sockfd, F_GETFL, 0);
		fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	}
	return sockfd;
}

string Helper::sendAndGetData(const string &ip, const int port,
							  const string &msg) {
	int n;
	char buffer[MAX_BUFFER_SIZE];
	cout << "Helper::sendAndGetData: " << ip << ":" << port << endl;
	int fd = connectToNode(ip, port);

	cout << "Helper::sendAndGetData : fd: " << fd << endl;
	n = write(fd, msg.c_str(), msg.length());
	if (n > 0 and static_cast<size_t>(n) != msg.length()) {
		cerr << "Helper::sendAndGetData: Error sending data" << endl;
	}
	memset(buffer, 0, MAX_BUFFER_SIZE);
	read(fd, buffer, MAX_BUFFER_SIZE);
	cout << "Helper::sendAndGetData : Message received \"" << string(buffer)
		 << "\"" << endl;
	close(fd);
	return string(buffer);
}

string Helper::sha256(const string &str) {
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, str.c_str(), str.size());
	SHA256_Final(hash, &sha256);
	stringstream ss;
	for (unsigned char i : hash) {
		ss << hex << setw(2) << setfill('0') << (int)i;
	}
	return ss.str();
}

string Helper::sha3_256(const std::string &input) {
	uint32_t digest_length = SHA256_DIGEST_LENGTH;
	const EVP_MD *algorithm = EVP_sha3_256();
	auto *digest = static_cast<uint8_t *>(OPENSSL_malloc(digest_length));
	EVP_MD_CTX *context = EVP_MD_CTX_new();
	EVP_DigestInit_ex(context, algorithm, nullptr);
	EVP_DigestUpdate(context, input.c_str(), input.size());
	EVP_DigestFinal_ex(context, digest, &digest_length);
	stringstream ss;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		ss << hex << setw(2) << setfill('0') << (int)digest[i];
	}
	OPENSSL_free(digest);
	return ss.str().substr(0, 16);
}