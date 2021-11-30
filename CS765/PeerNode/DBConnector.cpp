//
// Created by Pranav on 09-11-2020.
//

#include "DBConnector.h"

DBConnector::DBConnector(/* args */) = default;

DBConnector::~DBConnector() { mdbConnect->disconnect(); }

optional<Block> DBConnector::searchBlock(const Block &blk) {
    string sql = "SELECT HASH,HEIGHT,PREVIOUSBLOCKHASH,MERKLEROOT,TIME FROM " +
                 mtableName + " WHERE HASH='" + blk.getPreviousBlockHash() +
                 "';";
    pqxx::nontransaction transaction(*mdbConnect);
    pqxx::result Result(transaction.exec(sql));
    if (Result.begin() == Result.end()) {
        cout << "DBConnector::searchBlock: No such block" << endl;
        return nullopt;
    }

    pqxx::result::const_iterator c = Result.begin();
    cout << "HASH= " << c[0].as<string>() << endl;
    cout << "HEIGHT= " << c[1].as<int>() << endl;

    auto prevBlockHash = c[2].as<string>();
    auto merkleRoot = c[3].as<string>();
    int time = c[4].as<int>();

    cout << "PREVIOUSBLOCKHASH = " << prevBlockHash << endl;
    cout << "MERKLEROOT = " << merkleRoot << endl;
    cout << "TIME = " << time << endl;
    Block prevBlock(prevBlockHash, merkleRoot, time);
    return prevBlock;
}

bool DBConnector::insertBlock(const Block &blk) {
    string sql = "INSERT INTO " + mtableName +
                 "(HASH,HEIGHT,PREVIOUSBLOCKHASH,MERKLEROOT,TIME)"
                 " VALUES('" +
                 blk.getThisBlockHash() + "'," +
                 to_string(blk.getHeight()) + ",'" +
                 blk.getPreviousBlockHash() + "','" + blk.getMerkleRoot() +
                 "'," + to_string(blk.getTimeStamp()) + ");";
    if (not mdbConnect->is_open()) {
        cerr << "mdbConnector::initialize : Can't open database" << endl;
        return false;
    }
    try {
        pqxx::work transaction(*mdbConnect);
        transaction.exec(sql);
        transaction.commit();
//        cout << "DBConnector::insertBlock: Block inserted successfull: " << blk << endl;
    } catch (const std::exception &e) {
        cerr << "DBConnector::insertBlock: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DBConnector::initialize() {
    string sql;
    try {
        mdbConnect = new pqxx::connection(
                "dbname=pranav user=pranav hostaddr=127.0.0.1 port=5432");
        if (mdbConnect->is_open()) {
            cout << "DBConnector::initialize: Successfully connected to: " << mdbConnect->dbname()
                 << endl;
        } else {
            cerr << "DBConnector::initialize : Can't open database" << endl;
            return false;
        }
    } catch (const std::exception &e) {
        cerr << "DBConnector::initialize: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
    return true;
}

bool DBConnector::createTable(const string &tableName) {
    string sql = "CREATE TABLE IF NOT EXISTS " + tableName + "(" +
                 "HASH VARCHAR(" + to_string(Block::HASH_LENGTH) +
                 ") NOT NULL PRIMARY KEY, HEIGHT INT NOT NULL, PREVIOUSBLOCKHASH VARCHAR(" +
                 to_string(Block::HASH_LENGTH) + ") NOT NULL, MERKLEROOT VARCHAR(" +
                 to_string(Block::MERKLE_ROOT_LENGTH) + ") NOT NULL, TIME INT NOT NULL);";
    mtableName = tableName;
    if (!mdbConnect->is_open()) {
        cerr << "mdbConnector::initialize() : Can't open database" << endl;
        return false;
    }

    try {
        pqxx::work transaction(*mdbConnect);

        transaction.exec(sql);
        transaction.commit();
        cout << "DBConnector::createTable: Table created successfully" << endl;
    } catch (const std::exception &e) {
        cerr << "DBConnector::createTable: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool DBConnector::dropTable(const string &tableName) {
    string sql = "DROP TABLE " + tableName;

    mtableName = tableName;
    if (!mdbConnect->is_open()) {
        cerr << "DBConnector::initialize() : Can't open database" << endl;
        return false;
    }
    try {
        pqxx::work transaction(*mdbConnect);
        transaction.exec(sql);
        transaction.commit();

    } catch (const std::exception &e) {
    }
    return true;
}