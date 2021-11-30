//
// Created by Pranav on 09-11-2020.
//

#ifndef PEERNODE_DBCONNECTOR_H
#define PEERNODE_DBCONNECTOR_H



#include <optional>
#include <pqxx/pqxx>
#include <string>

#include "Block.h"

using namespace std;

class DBConnector {
private:
    pqxx::connection* mdbConnect{};
    string mtableName;

public:
    DBConnector();
    ~DBConnector();
    bool initialize();
    bool createTable(const string& tableName);
    bool dropTable(const string& tableName);

    optional<Block> searchBlock(const Block& blk);

    // Check height before inserting
    bool insertBlock(const Block& blk);
};


#endif //PEERNODE_DBCONNECTOR_H
