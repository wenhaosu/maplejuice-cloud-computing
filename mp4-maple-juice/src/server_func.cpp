/**
 * server_func.cpp
 * Implementation of functions in server_func.h.
 */

#include "server_func.h"

/// Uncomment this line to enter debug mode
#define DEBUG_MODE

#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else

#include <unistd.h>

#define GetCurrentDir getcwd
#endif

std::string current_working_directory() {
    char *cwd = GetCurrentDir(nullptr, 0);
    std::string working_directory(cwd);
    std::free(cwd);
    return working_directory;
}

server::server() {
    /// Check whether we can get current machine's IP
    if (get_my_ip_address() == "Unable to get IP Address")
        throw runtime_error("Failed in getting current machine's IP address");

    /// Check whether system function is available.
    if (!system(nullptr))
        throw runtime_error("System is not available\n");

    this->query_port = QUERY_PORT;
    this->hb_port = HB_PORT;
    this->sdfs_port = SDFS_PORT;
    this->mj_port = MJ_PORT;
    this->my_ip_address = get_my_ip_address();
    /// Initially every machine is not joined to the group.
    this->is_joined = false;
    this->last_send_heartbeat_time = get_curr_timestamp_milliseconds();
    curr_dir = current_working_directory();
    remove(LOG_FILE_PATH);
    server::init_files_path(SDFS_PATH);
    server::init_files_path(FETCHED_PATH);

#ifdef DEBUG_MODE
    cout << "### My ip address is " << this->my_ip_address << endl;
    cout << "### Indicator ip address is " << indicator_ip << endl;
#endif
    write_log_file("My ip address is: " + this->my_ip_address, 0);
    write_log_file("Indicator ip address is: " + indicator_ip, 0);
}

void make_server_sock_addr(struct sockaddr_in *addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(port);
}

void server::console_message() {
    if (this->is_joined) {
        cout << ">>> Currently joined the group. Please enter the following command:" << endl;
        cout << "==================================================================" << endl;
        cout << "'leave': leave the group." << endl;
    } else {
        cout << ">>> Currently not in the group. Please enter the following command:" << endl;
        cout << "==================================================================" << endl;
        cout << "'join': join the group." << endl;
    }
    cout << "'print': print current machine status." << endl;
    cout << "'exit': end the program." << endl;
    cout << "==================================================================" << endl;
}

int server::run_server() {
    /// Release TCP and UDP listeners.
    thread(&server::run_tcp_query_receiver, this).detach();
    thread(&server::run_tcp_sdfs_receiver, this).detach();
    thread(&server::run_tcp_maple_juice_receiver, this).detach();
    thread(&server::run_udp_receiver, this).detach();
    thread(&server::heartbeat_sender, this).detach();
    thread(&server::failure_detector, this).detach();
    if (my_ip_address == indicator_ip)
        thread(&server::run_maple_juice_handler, this).detach();

    string input;
    console_message();

    /// Waiting for user console input.
    while (getline(std::cin, input)) {
        cout << endl;
        if (input.empty()) continue;
        if (input == "exit") {
            write_log_file("User typed 'exit'", 0);
            write_log_file("Exit the system", 0);
            return 0;
        } else if (input == "join" && !this->is_joined) {
            write_log_file("User typed 'join'", 0);
            handle_join_group();
        } else if (input == "leave" && this->is_joined) {
            write_log_file("User typed 'leave'", 0);
            handle_leave_group();
        } else if (input == "print") {
            write_log_file("User typed 'print'", 0);
            print_curr_membership_list();
        } else console_message();
    }
    return 0;
}

void server::run_tcp_query_receiver() {
    /// Initialize socket connection.
    int server_fd, sock;
    struct sockaddr_in address{};
    int yes = 1;
    int addrlen = sizeof(address);

    /// Create socket file descriptor.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw runtime_error("Failure in create TCP socket");

    /// Attach socket to the given port number.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        throw runtime_error("Failure in TCP setsockopt");

    make_server_sock_addr(&address, query_port);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        throw runtime_error("Failure in bind TCP socket");

    if (listen(server_fd, 3) < 0)
        throw runtime_error("Failure in listen TCP socket");

    /// Start receiving message.
    while (true) {
        sock = accept(server_fd, (struct sockaddr *) &address, (socklen_t * ) & addrlen);
        if (sock == -1)
            throw runtime_error("Bad socket connection");
        char buffer[MAX_BUFFER_SIZE] = {0};
        read(sock, buffer, MAX_BUFFER_SIZE);
        string query(buffer), query_type;
        stringstream ss(query);
        ss >> query_type;
        if (query_type == "grep")
            thread(&server::handle_grep_request, this, sock, query).detach();
        else if (query_type == "put")
            thread(&server::handle_put_request, this, sock, query).detach();
        else if (query_type == "get")
            thread(&server::handle_get_request, this, sock, query).detach();
        else if (query_type == "delete")
            thread(&server::handle_delete_request, this, sock, query).detach();
        else if (query_type == "ls")
            thread(&server::handle_ls_request, this, sock, query).detach();
        else if (query_type == "store")
            thread(&server::handle_store_request, this, sock, query).detach();
        else if (query_type == "maple" || query_type == "juice") {
            if (my_ip_address == indicator_ip) {
                maple_juice_requests_lock.lock();
                maple_juice_requests.push(make_pair(query, sock));
                maple_juice_requests_lock.unlock();
            }
        } else close(sock);
    }
}


void server::run_tcp_sdfs_receiver() {
    /// Initialize socket connection.
    int server_fd, sock;
    struct sockaddr_in address{};
    int yes = 1;
    int addrlen = sizeof(address);

    /// Create socket file descriptor.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw runtime_error("Failure in create TCP socket");

    /// Attach socket to the given port number.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        throw runtime_error("Failure in TCP setsockopt");

    make_server_sock_addr(&address, sdfs_port);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        throw runtime_error("Failure in bind TCP socket");

    if (listen(server_fd, 3) < 0)
        throw runtime_error("Failure in listen TCP socket");

    /// Start receiving message.
    while (true) {
        sock = accept(server_fd, (struct sockaddr *) &address, (socklen_t * ) & addrlen);
        if (sock == -1)
            throw runtime_error("Bad socket connection");
        char buffer[MAX_BUFFER_SIZE] = {0};
        read(sock, buffer, MAX_BUFFER_SIZE);
        string query(buffer), query_type;
        stringstream ss(query);
        ss >> query_type;
        if (query_type == "put_start")
            thread(&server::put_query_receiver, this, sock, query).detach();
        else if (query_type == "get_start")
            thread(&server::get_query_receiver, this, sock, query).detach();
        else if (query_type == "delete_start")
            thread(&server::delete_query_receiver, this, sock, query).detach();
        else if (query_type == "exist")
            thread(&server::file_exist_request, this, sock, query).detach();
        else if (query_type == "check_time")
            thread(&server::check_time_valid, this, sock, query).detach();
        else if (query_type == "prefix_exist")
            thread(&server::handle_prefix_check_exist, this, sock, query).detach();
        else if (query_type == "prefix_delete")
            thread(&server::handle_prefix_delete, this, sock, query).detach();
        else close(sock);
    }
}

void server::run_tcp_maple_juice_receiver() {
    /// Initialize socket connection.
    int server_fd, sock;
    struct sockaddr_in address{};
    int yes = 1;
    int addrlen = sizeof(address);

    /// Create socket file descriptor.
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        throw runtime_error("Failure in create TCP socket");

    /// Attach socket to the given port number.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        throw runtime_error("Failure in TCP setsockopt");

    make_server_sock_addr(&address, mj_port);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        throw runtime_error("Failure in bind TCP socket");

    if (listen(server_fd, 3) < 0)
        throw runtime_error("Failure in listen TCP socket");

    /// Start receiving message.
    while (true) {
        sock = accept(server_fd, (struct sockaddr *) &address, (socklen_t * ) & addrlen);
        if (sock == -1)
            throw runtime_error("Bad socket connection");
        char buffer[MAX_BUFFER_SIZE] = {0};
        read(sock, buffer, MAX_BUFFER_SIZE);
        string query(buffer), query_type;
        stringstream ss(query);
        ss >> query_type;
        if (query_type == "maple_start")
            thread(&server::maple_task_processor, this, sock, query).detach();
        else if (query_type == "juice_start")
            thread(&server::juice_task_processor, this, sock, query).detach();
        else close(sock);
    }
}

void server::run_udp_sender(string target_ip_addr, vector <Member> messages) {
    int sock_fd, num_bytes;
    struct sockaddr_in serveraddr{};
    struct hostent *server;

    /// Create socket file descriptor.
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0)
        throw runtime_error("Failure in create UDP socket");

    /// Get the target server's DNS entry.
    server = gethostbyname(target_ip_addr.c_str());
    if (server == nullptr)
        throw runtime_error("Failure in find target host");

    /// Build the server's Internet address.
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(hb_port);

    /// Construct the message item to send.
    Member packet[messages.size()];

    for (int i = 0; (size_t) i < messages.size(); i++)
        packet[i] = messages[i];

    char buf[sizeof(packet)];
    bzero(buf, sizeof(packet));

    memcpy(buf, &packet, sizeof(packet));

    /// Send the message to the server.
    num_bytes = sendto(sock_fd, buf, sizeof(buf), 0,
                       reinterpret_cast<const sockaddr *>(&serveraddr), sizeof(serveraddr));

    if (num_bytes < 0)
        throw runtime_error("Failure in sending message");

    close(sock_fd);
}

void server::run_udp_receiver() {
    /// Initialize socket connection.
    int server_fd, clientlen;
    struct sockaddr_in serveraddr{}, clientaddr{};
    int yes = 1;
    clientlen = sizeof(clientaddr);

    /// Create socket file descriptor.
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        throw runtime_error("Failure in create UDP socket");

    /// Attach socket to the given port number.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
        throw runtime_error("Failure in UDP setsockopt");

    make_server_sock_addr(&serveraddr, hb_port);

    if (bind(server_fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        throw runtime_error("Failure in bind UDP socket");

    /// Start receiving message.
    while (true) {
        char buffer[MAX_BUFFER_SIZE] = {0};
        int num_bytes = recvfrom(server_fd, (char *) buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &clientaddr,
                                 reinterpret_cast<socklen_t *>(&clientlen));
        if (num_bytes < 0) continue;

        /// Upon receive the membership list from other machine, first decode it as a vector of Member.
        vector <Member> received_list = decode_membership_list(buffer, num_bytes);

        /// The message type is encoded in the first Member's updated_time attribute.
        int message_type = received_list[0].updated_time;
        vector <Member> message;

        switch (message_type) {
            case JOIN: /// Message type is "join" for 0.
                write_log_file("Received 'join' from " + string(inet_ntoa(clientaddr.sin_addr)), 0);
                /// Add the newly joined machine to my local membership list.
                update_membership(received_list);
                /// Prepare to send my local membership list to the newly joined machine.
                message = get_membership_list_as_vector();
                /// Change message type to "join_success".
                message[0].updated_time = JOIN_SUCCESS;
                message = encode_membership_list(message);
                /// Send my local membership list to the newly joined machine.
                run_udp_sender(inet_ntoa(clientaddr.sin_addr), message);
                this_thread::sleep_for(chrono::nanoseconds(2000));
                handle_sdfs_join_rearrange();
                break;
            case LEAVE:
            case FAILURE:
            case ANNOUNCE:
            case HEARTBEAT:
                thread(&server::update_membership, this, received_list).detach();
                break;
            case JOIN_SUCCESS: /// Message type is "join_success" for 3.
                write_log_file("Received 'join success' from " + string(inet_ntoa(clientaddr.sin_addr)), 0);
                /// Update my local membership list according to indicator's list.
                this->is_joined = true;
                thread(&server::update_membership, this, received_list).detach();
                /// Refresh console message.
                console_message();
                break;
            default:
                cerr << "### No such type of message: " << message_type << endl;
                break;
        }
    }
}

int server::handle_grep_request(int sock, string grep_command) {
    try {
        /// Run grep command based on query message.
        string grep_result = grep(grep_command);
        const char *res = grep_result.c_str();

        /// Send back the result of query.
        send(sock, res, strlen(res), 0);

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
