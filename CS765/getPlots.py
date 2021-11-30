import psycopg2
import sys
import matplotlib.pyplot as plt

mGenesisBlockHash = "0000000000009e1c"


def open_connection():
    try:
        connection = psycopg2.connect(user="pranav",
                                      host="127.0.0.1",
                                      port="5432",
                                      database="pranav")
        cursor = connection.cursor()
        return cursor
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_table_names(cursor):
    try:
        query = "SELECT table_schema,table_name FROM information_schema.tables WHERE table_schema = 'public'"
        cursor.execute(query)
        list_tables = cursor.fetchall()
        tables = []
        for t_name_table in list_tables:
            tables.append(t_name_table[1])
        return tables
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_max_height_in_table(cursor, table):
    try:
        query = "SELECT MAX(height) FROM " + table
        cursor.execute(query)
        max_height = cursor.fetchall()
        return max_height[0][0]
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_block_with_height(cursor, table, height):
    try:
        query = "SELECT * FROM " + table + " WHERE height=\'"+str(height)+"\'"
        cursor.execute(query)
        max_block = cursor.fetchall()
        return max_block

    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_block_with_height_and_hash(table, height, hash):
    if (hash == mGenesisBlockHash):
        return None
    try:
        query = "SELECT * FROM " + table + " WHERE height=" + \
            str(height)+" AND hash=\'"+hash+"\'"
        cursor.execute(query)
        max_block = cursor.fetchall()
        return max_block[0]
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_mining_power_utilization(cursor, table):
    try:
        count_query = "SELECT COUNT(*) FROM " + table
        max_query = "SELECT MAX(height) FROM " + table
        cursor.execute(count_query)
        count = cursor.fetchall()
        print(count)
        cursor.execute(max_query)
        max_height = cursor.fetchall()
        return max_height[0][0]/count[0][0]
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_longest_chains(cursor, table):
    try:
        chains = []
        longest_blockchain = {}
        max_height = get_max_height_in_table(cursor, table)
        max_blocks = get_block_with_height(cursor, table, max_height)
        print(max_height, max_blocks)
        for max_block in max_blocks:
            print(max_block)
            current_height = max_height
            longest_blockchain[max_block[0]] = max_block[1:]
            hash = max_block[2]
            while True:
                current_height -= 1
                prev_block = get_block_with_height_and_hash(
                    table, current_height, hash)
                if prev_block is None:
                    chains.append(longest_blockchain.copy())
                    longest_blockchain.clear()
                    break
                else:
                    hash = prev_block[2]
                    longest_blockchain[prev_block[0]] = prev_block[1:]
        return chains
    except (Exception, psycopg2.Error) as error:
        print(error)
        exit()


def get_mining_util_stat(cursor, table_name):
    try:
        tables = get_table_names(cursor)
        tables = [x for x in tables if x.startswith(table_name)]
        print("Tables:", tables)
        values = []
        for table in tables:
            y = get_mining_power_utilization(cursor, table)
            x = table.split('_')[-1]
            values.append((x, y))
        return values
    except Exception as error:
        print(error)


def get_mining_util_stats(cursor, tables):
    try:
        values = {'5': 0, '10': 0.0, '15': 0.0, '20': 0.0}
        for table in tables:
            stats = get_mining_util_stat(cursor, table)
            for stat in stats:
                values[stat[0]] += stat[1]
        x = []
        y = []
        for k in values:
            x.append(k)
            values[k] /= 10
            y.append(values[k])
        plt.plot(x, y)
        plt.xlabel('Inter-arrival time')
        plt.ylabel('Mining Power Utilization')
        plt.savefig("20"+"_pow")
    except Exception as error:
        print(error)


def get_blocks_from_file(file_name):
    lines = []
    with open(file_name, 'r') as fd:
        lines = fd.readlines()
    lines = list(map(lambda x: x.split(" : ")[3], list(
        map(lambda x: x.strip("\n"), lines))))
    return lines


def get_fraction(cursor, file_name, table_name):
    try:
        chains = get_longest_chains(cursor, table_name)
        # print(len(chains))
        # print(chains)
        max_count = 0
        blocks = get_blocks_from_file(file_name)
        # print(blocks)
        # print(len(chains))
        for chain in chains:
            temp_count = 0
            # print(chain.keys())
            for block in blocks:
                # print(block)
                if block in chain.keys():
                    temp_count += 1
                    # print(temp_count)
            max_count = max(max_count, temp_count)
            # print(max_count)
        return (table_name.split("_")[-1], max_count/get_max_height_in_table(cursor, table_name))
    except Exception as error:
        print(error)


def get_fractions(cursor, file_names, table_name):
    tables = get_table_names(cursor)
    file_names.sort()
    tables = [x for x in tables if x.startswith(table_name)]
    tables.sort()
    print(tables)
    zips = list(zip(tables, file_names))
    zips.sort(key=lambda x: int(x[0].split("_")[-1]))
    print(zips)
    x = []
    y = []
    for z in zips:
        a, b = get_fraction(cursor, z[1], z[0])
        print(a, b)
        x.append(a)
        y.append(b)
    plt.plot(x, y)
    plt.xlabel('Inter-arrival time')
    plt.ylabel('Fraction')
    plt.savefig("20"+"_frac")


if __name__ == "__main__":
    cursor = open_connection()
    if sys.argv[1] == "power":
        tables = ["chain_127_0_0_1_10000",
                  "chain_127_0_0_1_10001", "chain_127_0_0_1_10002"]
        # "chain_127_0_0_1_10003", "chain_127_0_0_1_10004", ]  # "chain_127_0_0_1_10005",
        #   "chain_127_0_0_1_10006", "chain_127_0_0_1_10007", "chain_127_0_0_1_10008",
        #   "chain_127_0_0_1_10009"]
        get_mining_util_stats(cursor, tables)
    elif sys.argv[1] == "fraction":
        file_names = ["./PeerNode/build/127.0.0.1_10002_5.blocks",
                      "./PeerNode/build/127.0.0.1_10002_10.blocks",
                      "./PeerNode/build/127.0.0.1_10002_15.blocks",
                      "./PeerNode/build/127.0.0.1_10002_20.blocks"]
        get_fractions(cursor, file_names, sys.argv[2])
        # get_blocks_from_file(sys.argv[3])
