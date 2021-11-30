/*
#include <iostream>
#include <pqxx/pqxx>

using namespace std;
using namespace pqxx;

int main(int argc, char* argv[]) {
	char* sql;

	try {
		connection C(
			"dbname = BlockChain user = shailendra \
	  hostaddr = 127.0.0.1 port = 5432");
		if (C.is_open()) {
			cout << "Opened database successfully: " << C.dbname() << endl;
		} else {
			cout << "Can't open database" << endl;
			return 1;
		}

sql =
	"CREATE TABLE IF NOT EXISTS BLOCKS("
	"HEIGHT         INT     NOT NULL,"
	"PREVIOSBLOCKHASH           TEXT    NOT NULL,"
	"MERKLEROOT     TEXT     NOT NULL,"
	"TIME         INT     NOT NULL );";


work W(C);

W.exec(sql);
W.commit();
cout << "Table created successfully" << endl;
C.disconnect();
}
catch (const std::exception& e) {
	cerr << e.what() << std::endl;
	return 1;
}

return 0;
}
*/

#include <postgresql/libpq-fe.h>
#include <stdio.h>

#include <string>

int main() {
	PGconn *conn;
	PGresult *res;
	int rec_count;
	int row;
	int col;

	conn = PQconnectdb("dbname=BlockChain host=localhost user=shailendra");

	if (PQstatus(conn) == CONNECTION_BAD) {
		puts("We were unable to connect to the database");
		exit(0);
	}
}
