//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_HELPER_H
#define PEERNODE_HELPER_H


#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>

using namespace std;

class Helper {
private:
    static const int MAX_BUFFER_SIZE = 4096;

public:
    Helper(/* args */) = delete;
    ~Helper() = delete;

    static int getSystemTimeStamp();
    static bool sendDataToNode(int fd, const string &msg);
    static int connectToNode(const string &ip, int port,
                             bool nonBlocking = false);
    static string sendAndGetData(const string &ip, int port,
                                 const string &msg);
    static void setTimeoutForSocket(int fd);
    static string getRandomString(int length);

    // Hashing Function
    static string sha256(const string &str);
    static string sha3_256(const std::string &input);

};



#endif //PEERNODE_HELPER_H
