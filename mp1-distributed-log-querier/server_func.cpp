/**
 * server_func.cpp
 * Implementation of functions in server_func.h.
 */

#include "server_func.h"

/// The maximum size of single buffer
#define MAX_BUFFER_SIZE 1024

/// Uncomment this line to enter debug mode
//#define DEBUG_MODE

server::server(int port_num) {
    this->port_num = port_num;
}

void make_server_sock_addr(struct sockaddr_in *addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(port);
}

int server::run_server() {

    /// Initialize socket connection.
    int server_fd, sock;
    struct sockaddr_in address{};
    int yes = 1;
    int addrlen = sizeof(address);

    /// Create socket file descriptor.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw runtime_error("Failure in create socket");

    /// Attach socket to the given port number.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        throw runtime_error("Failure in setsockopt");

    make_server_sock_addr(&address, port_num);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        throw runtime_error("Failure in bind");

    if (listen(server_fd, 3) < 0)
        throw runtime_error("Failure in listen");

    /// Start receiving message.
    while (true) {
        sock = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen);
        auto t = thread(&server::handle_connection, this, sock);
        t.detach();
    }
}

int server::handle_connection(int sock) {
    char buffer[MAX_BUFFER_SIZE] = {0};
    try {
        if (sock == -1)
            throw runtime_error("Bad socket connection");

        read(sock, buffer, MAX_BUFFER_SIZE);

#ifdef DEBUG_MODE
        cout << "Grep message received: " << buffer << endl;
#endif

        /// Run grep command based on query message.
        string grep_result = grep(buffer);
        const char *res = grep_result.c_str();

        /// Send back the result of query.
        send(sock, res, strlen(res), 0);

#ifdef DEBUG_MODE
        cout << "Grep result sent." << endl;
#endif

        close(sock);
        return 0;
    } catch (runtime_error &e) {
        /// If error occurs, then send an empty string to client and print the error message on server.
        std::cerr << "error: " << e.what() << std::endl;
        string empty = "\0";
        const char *res = empty.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
        return -1;
    }
}
