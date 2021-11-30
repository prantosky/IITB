cmake_minimum_required(VERSION 3.16)
project(PeerNode)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads)
find_package(OpenSSL REQUIRED)
find_library(PQXX_LIB pqxx REQUIRED)


add_executable(PeerNode PeerNode.cpp ThreadPool.cpp ThreadPool.h Block.cpp Block.h BlockChain.cpp BlockChain.h Handler.cpp Handler.h Formatter.cpp Formatter.h DBConnector.cpp DBConnector.h Helper.cpp Helper.h)

target_link_libraries(PeerNode Threads::Threads OpenSSL::Crypto ${PQXX_LIB})

