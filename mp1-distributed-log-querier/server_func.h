/**
 * server_func.cpp
 * Define functions used in server.
 */

#ifndef SERVER_FUNC_H
#define SERVER_FUNC_H

#include "grep.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <cstring>
#include <thread>
#include <stdexcept>

class server {
private:
    /// The port that server is listening.
    int port_num;

    /**
     * Thread for handle connection from client.
     *
     * Parameters:
     *      sock: The socket parameter for connection.
     */
    int handle_connection(int sock);

public:

    /**
     * Initializer for the server class.
     *
     * Parameters:
     *      port_num: The port that server is listening.
     */
    explicit server(int port_num = 0);

    /**
     * Run the initialized server.
     */
    int run_server();
};


#endif //SERVER_FUNC_H
