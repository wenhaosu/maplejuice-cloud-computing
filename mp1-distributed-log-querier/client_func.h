/**
 * client_func.cpp
 * Define functions used in client.
 */

#ifndef CLIENT_FUNC_H
#define CLIENT_FUNC_H

#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <iterator>
#include <fstream>
#include <cassert>
#include <sstream>
#include <unordered_map>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <queue>
#include <algorithm>

#define MAX_VM_NUM 10

using namespace std;

class client {
private:
    /// The port that server is listening.
    int port_num;

    /// Mutex for cout printing and query result writing.
    mutex cout_lock;
    mutex write_result;

    /// The string queue that stores all the user input queries for processing.
    queue<string> queries;
    mutex queries_lock;

    /// The structure for strong vm IP Addresses.
    /// Key: vm id. Value: IP Address.
    unordered_map<int, string> ip_addresses = {
            {0, "172.22.154.5"},
            {1, "172.22.156.5"},
            {2, "172.22.152.6"},
            {3, "172.22.154.6"},
            {4, "172.22.156.6"},
            {5, "172.22.152.11"},
            {6, "172.22.154.7"},
            {7, "172.22.156.7"},
            {8, "172.22.152.12"},
            {9, "172.22.154.8"}
    };

    /// The bool vector that stores the status (alive or died) for vm servers.
    vector<bool> ip_status;
    mutex ip_status_lock;

    /// The bool vector that stores whether the query sent to each machine is finished.
    vector<bool> query_status;
    mutex query_status_lock;

    /**
     * Thread for sending query tasks to the specified server.
     *
     * Parameters:
     *      vm_id: The id of server, which is the key of ip_addresses.
     *      input: The query message.
     *
     * Returns:
     *      Return 0 on success, -1 on failure.
     */
    int send_grep_query(int vm_id, string input);

    /**
     * Thread for checking queries, create task threads if there are standby queries.
     */
    void process_queries();

public:

    /**
     * Initializer for the client class.
     *
     * Parameters:
     *      port_num: The port that server is listening.
     */
    explicit client(int port_num = 0);

    /**
     * Run the initialized client.
     */
    int run_client();
};

#endif //CLIENT_FUNC_H
