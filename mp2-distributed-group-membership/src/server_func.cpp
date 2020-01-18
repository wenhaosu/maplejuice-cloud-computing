/**
 * server_func.cpp
 * Implementation of functions in server_func.h.
 */


#include "server_func.h"

/// The maximum size of single buffer
#define MAX_BUFFER_SIZE 1024

/// Uncomment this line to enter debug mode
#define DEBUG_MODE

server::server(int port_num) {
    if (get_my_ip_address() == "Unable to get IP Address")
        throw runtime_error("Failed in getting current machine's IP address");
    this->port_num = port_num;
    this->my_ip_address = get_my_ip_address();
    /// Initially every machine is not joined to the group.
    this->is_joined = false;
    this->last_send_heartbeat_time = get_curr_timestamp_milliseconds();
    remove(LOG_FILE_PATH);
#ifdef DEBUG_MODE
    cout << "### My ip address is " << get_my_ip_address() << endl;
    cout << "### Indicator ip address is " << indicator_ip << endl;
#endif
    write_log_file("My ip address is: " + get_my_ip_address(), 0);
    write_log_file("Indicator ip address is: " + indicator_ip, 0);
}

void make_server_sock_addr(struct sockaddr_in *addr, int port) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addr->sin_port = htons(port);
}

void server::console_message() {
    if (this->is_joined)
        cout << ">>> Currently joined the group, type 'leave' to leave the group, type 'exit' to end program:" << endl;
    else
        cout << ">>> Currently not in the group, type 'join' to join the group, type 'exit' to end program:" << endl;
}

vector<Member> server::get_membership_list_as_vector() {
    vector<Member> message;
    membership_list_lock.lock();
    for (const auto &item : membership_list)
        message.push_back(item.second);
    membership_list_lock.unlock();
    return message;
}

vector<Member> server::encode_membership_list(vector<Member> message) {
    for (int i = 0; (size_t) i < message.size(); i++) {
        message[i].time_stamp = htonl(message[i].time_stamp);
        message[i].updated_time = htonl(message[i].updated_time);
    }
    return message;
}


void server::print_curr_membership_list() {
    membership_list_lock.lock();
    write_membership_list_to_log_file();
    cout << "### My membership list is now: " << endl;
    for (const auto &m : membership_list)
        cout << m.first << " " << m.second.time_stamp << " " << m.second.updated_time << endl;
    membership_list_lock.unlock();
    auto listen_list = find_my_listen_targets();
    auto send_list = find_my_send_targets();
    cout << "### I should listen:" << endl;
    for (const auto &m : listen_list)
        cout << m << endl;
    cout << "### I should send to:" << endl;
    for (const auto &m : send_list)
        cout << m << endl;
}


vector<Member> server::decode_membership_list(char *raw_data, int num_bytes) {
    vector<Member> received_list;
    char *it = raw_data;
    while (num_bytes) {
        auto *member = (Member *) it;
        member->time_stamp = ntohl(member->time_stamp);
        member->updated_time = ntohl(member->updated_time);
        received_list.push_back(*member);
        it += sizeof(Member);
        num_bytes -= sizeof(Member);
    }
    return received_list;
}

uint64_t server::get_curr_timestamp_milliseconds() {
    return (uint64_t) chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
}

string server::get_my_ip_address() {
    string ipAddress = "Unable to get IP Address";
    struct ifaddrs *interfaces = nullptr;
    struct ifaddrs *temp_addr = nullptr;
    int success = 0;
    /// Retrieve the current interfaces - returns 0 on success.
    success = getifaddrs(&interfaces);
    if (success == 0) {
        /// Loop through linked list of interfaces.
        temp_addr = interfaces;
        while (temp_addr != nullptr) {
            if (temp_addr->ifa_addr->sa_family == AF_INET)
                /// Check if interface is en0 which is the wifi connection on the iPhone.
                if (strcmp(temp_addr->ifa_name, "en0") != 0)
                    ipAddress = inet_ntoa(((struct sockaddr_in *) temp_addr->ifa_addr)->sin_addr);
            temp_addr = temp_addr->ifa_next;
        }
    }
    /// Free memory.
    freeifaddrs(interfaces);
    return ipAddress;
}

int server::run_server() {
    /// Release TCP and UDP listeners.
    thread(&server::run_tcp_receiver, this).detach();
    thread(&server::run_udp_receiver, this).detach();
    thread(&server::heartbeat_sender, this).detach();
    thread(&server::failure_detector, this).detach();

    string input;
    console_message();

    /// Waiting for user console input.
    while (getline(std::cin, input)) {
        cout << endl;
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

void server::run_tcp_receiver() {
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

    make_server_sock_addr(&address, port_num);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0)
        throw runtime_error("Failure in bind TCP socket");

    if (listen(server_fd, 3) < 0)
        throw runtime_error("Failure in listen TCP socket");

    /// Start receiving message.
    while (true) {
        sock = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen);
        auto t = thread(&server::handle_grep_request, this, sock);
        t.detach();
    }
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

    make_server_sock_addr(&serveraddr, port_num);

    if (bind(server_fd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        throw runtime_error("Failure in bind UDP socket");

    /// Start receiving message.
    while (true) {
        char buffer[MAX_BUFFER_SIZE] = {0};
        int num_bytes = recvfrom(server_fd, (char *) buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *) &clientaddr,
                                 reinterpret_cast<socklen_t *>(&clientlen));
        if (num_bytes < 0) continue;

        /// Upon receive the membership list from other machine, first decode it as a vector of Member.
        vector<Member> received_list = decode_membership_list(buffer, num_bytes);

        /// The message type is encoded in the first Member's updated_time attribute.
        int message_type = received_list[0].updated_time;
        vector<Member> message;

        switch (message_type) {
            case JOIN: /// Message type is "join" for 0.
#ifdef DEBUG_MODE
                cout << "### Received 'join' from " << inet_ntoa(clientaddr.sin_addr) << endl;
#endif
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
                break;
            case LEAVE:
            case FAILURE:
            case ANNOUNCE:
            case HEARTBEAT:
                thread(&server::update_membership, this, received_list).detach();
                break;
            case JOIN_SUCCESS: /// Message type is "join_success" for 3.
#ifdef DEBUG_MODE
                cout << "### Received 'join success' from " << inet_ntoa(clientaddr.sin_addr) << endl;
#endif
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

int server::handle_grep_request(int sock) {
    char buffer[MAX_BUFFER_SIZE] = {0};
    try {
        if (sock == -1)
            throw runtime_error("Bad socket connection");
        read(sock, buffer, MAX_BUFFER_SIZE);

        /// Run grep command based on query message.
        string grep_result = grep(buffer);
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

void server::update_membership(vector<Member> received_list) {
    membership_list_lock.lock();
    bool to_send = false;
    auto m = received_list[0];
    if ((m.updated_time == ANNOUNCE && membership_list.find(m.ip_address) == membership_list.end())
        || m.updated_time == JOIN) {
        if (m.updated_time == ANNOUNCE) {
            write_log_file("Received 'announce' from " + string(m.ip_address), 0);
#ifdef DEBUG_MODE
            cout << "### Received 'announce' from " << m.ip_address << endl;
#endif
        }
        membership_list[m.ip_address] = m;
        membership_list[m.ip_address].updated_time = get_curr_timestamp_milliseconds();
        write_log_file(string(m.ip_address) + " added to my membership list", 1);
        m.updated_time = ANNOUNCE;
        to_send = true;
    } else if (m.updated_time == JOIN_SUCCESS) {
        for (auto item : received_list) {
            membership_list[item.ip_address] = item;
            membership_list[item.ip_address].updated_time = get_curr_timestamp_milliseconds();
            write_log_file(string(item.ip_address) + " added to my membership list", 1);
        }
    } else if (membership_list.find(m.ip_address) == membership_list.end() ||
               membership_list[m.ip_address].time_stamp >= m.time_stamp || !is_joined) {
        membership_list_lock.unlock();
        return;
    } else if (m.updated_time == FAILURE) {
        cout << "### Received 'failure' from " << m.ip_address << endl;
        write_log_file("Received 'failure' from " + string(m.ip_address), 0);
        membership_list.erase(m.ip_address);
        write_membership_list_to_log_file();
        to_send = true;
    } else if (m.updated_time == LEAVE) {
        cout << "### Received 'leave' from " << m.ip_address << endl;
        write_log_file("Received 'leave' from " + string(m.ip_address), 0);
        membership_list.erase(m.ip_address);
        write_membership_list_to_log_file();
        to_send = true;
    } else {
        if (membership_list[m.ip_address].updated_time == FAILURE ||
            membership_list[m.ip_address].updated_time == LEAVE) {
            /// means we need to erase the m from the failure, leave list
            write_log_file(string(m.ip_address) + " UNSUSPECT", 1);
            cout << "### Unsuspect " << m.ip_address << " from being failure" << endl;
            suspect_list.erase(m.ip_address);
            to_send = true;
        }
        membership_list[m.ip_address].time_stamp = m.time_stamp;
        membership_list[m.ip_address].updated_time = get_curr_timestamp_milliseconds();
    }
    membership_list_lock.unlock();
    if (to_send) send_one_entity(m);
}

void server::handle_join_group() {
    /// Create and send a membership list with only myself to indicator.
    Member join_entity{};
    strcpy(join_entity.ip_address, my_ip_address.c_str());
    join_entity.time_stamp = get_curr_timestamp_milliseconds();
    /// Change message type to "join".
    join_entity.updated_time = JOIN;
    vector<Member> message = encode_membership_list({join_entity});
    thread(&server::run_udp_sender, this, indicator_ip, message).detach();
}

void server::handle_leave_group() {
    /// Create and send a membership list with only myself to my heartbeat targets.
    Member leave_entity{};
    strcpy(leave_entity.ip_address, my_ip_address.c_str());
    leave_entity.time_stamp = get_curr_timestamp_milliseconds();
    /// Change message type to "leave".
    leave_entity.updated_time = LEAVE;
    vector<Member> message = encode_membership_list({leave_entity});
    vector<string> send_target = find_my_send_targets();
    for (auto &target : send_target)
        thread(&server::run_udp_sender, this, target, message).detach();
    membership_list_lock.lock();
    membership_list.clear();
    this->is_joined = false;
    membership_list_lock.unlock();
    console_message();
}

void server::run_udp_sender(string target_ip_addr, vector<Member> messages) {
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
    serveraddr.sin_port = htons(port_num);

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

void server::heartbeat_sender() {
    while (true) {
        if (membership_list.size() <= 1) continue;
        if (get_curr_timestamp_milliseconds() - last_send_heartbeat_time < HEARTBEAT_WAIT_MILLISECONDS) continue;
        last_send_heartbeat_time = get_curr_timestamp_milliseconds();
        Member heartbeat_entity{};
        strcpy(heartbeat_entity.ip_address, my_ip_address.c_str());
        heartbeat_entity.time_stamp = get_curr_timestamp_milliseconds();
        /// Change message type to "heartbeat".
        heartbeat_entity.updated_time = HEARTBEAT;
        send_one_entity(heartbeat_entity);
    }
}

vector<string> server::find_my_send_targets() {
    membership_list_lock.lock();
    vector<string> send_target;
    auto it = membership_list.find(my_ip_address);
    while (true) {
        it++;
        if (it == membership_list.end()) it = membership_list.begin();
        if (it == membership_list.find(my_ip_address) ||
            send_target.size() >= NUM_HEARTBEAT_SEND_TARGET)
            break;
        if (it->second.updated_time == LEAVE) continue;
        send_target.push_back(it->first);
    }
    membership_list_lock.unlock();
    return send_target;
}


vector<string> server::find_my_listen_targets() {
    membership_list_lock.lock();
    vector<string> listen_target;
    auto it = membership_list.rbegin();
    for (; it != membership_list.rend(); it++) {
        if (it->first == my_ip_address) break;
    }
    while (true) {
        it++;
        if (it == membership_list.rend()) it = membership_list.rbegin();
        if (it->first == my_ip_address || listen_target.size() >= NUM_HEARTBEAT_LISTEN_TARGET) break;
        if (it->second.updated_time == LEAVE) continue;
        listen_target.push_back(it->first);
    }
    membership_list_lock.unlock();
    return listen_target;
}

string get_time_str() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    string res = ctime(&in_time_t);
    res.erase(std::remove(res.begin(), res.end(), '\n'), res.end());
    return res;
}

void server::write_log_file(string log_message, int type) {
    log_file_lock.lock();
    ofstream myfile;
    myfile.open(LOG_FILE_PATH, std::ofstream::app);
    myfile << (type == 0 ? "### " : ">>> ") << "[" << get_time_str() << "] " << log_message << endl;
    myfile.close();
    log_file_lock.unlock();
}


void server::write_membership_list_to_log_file() {
    log_file_lock.lock();
    ofstream myfile;
    myfile.open(LOG_FILE_PATH, std::ofstream::app);
    myfile << ">>> " << "[" << get_time_str() << "] " << "My membership list is now: " << endl;
    for (const auto &m : membership_list)
        myfile << ">>> " << m.first << " - " << m.second.time_stamp << " - " << m.second.updated_time << endl;
    myfile.close();
    log_file_lock.unlock();
}


void server::send_one_entity(Member m) {
    vector<string> send_target = find_my_send_targets();
    vector<Member> message = encode_membership_list({m});
    for (auto &target : send_target)
        thread(&server::run_udp_sender, this, target, message).detach();
}

void server::failure_detector() {
    while (true) {
        if (membership_list.size() <= 1) continue;
        vector<string> listen_target = find_my_listen_targets();
        vector<Member> new_failure;
        membership_list_lock.lock();
        for (auto &target : listen_target) {
            /// First check whether a listened target is suspected, leave or has valid timestamp.
            if (get_curr_timestamp_milliseconds() - membership_list[target].updated_time >
                FAILURE_SUSPECT_WAIT_MILLISECONDS && membership_list[target].updated_time != FAILURE) {
                /// Means this target should be suspected.
                membership_list[target].updated_time = FAILURE;
                suspect_list[target] = membership_list[target];
                suspect_list[target].updated_time = get_curr_timestamp_milliseconds();
                write_log_file(string(membership_list[target].ip_address) + " may FAILURE", 1);
                cout << "### Suspect " << membership_list[target].ip_address << " to be failure" << endl;
            }
        }

        vector<string> suspect_erase_pool;

        /// Check whether a timeout message should be considered as failed and removed.
        for (auto &suspect : suspect_list) {
            if (get_curr_timestamp_milliseconds() - suspect.second.updated_time > TIMEOUT_ERASE_MILLISECONDS) {
                write_log_file(string(suspect.first) + " erased", 1);
                cout << "### Suspect " << suspect.first << " erased" << endl;
                if (membership_list[suspect.first].updated_time == FAILURE)
                    new_failure.push_back(membership_list[suspect.first]);
                suspect_erase_pool.push_back(suspect.first);
                membership_list.erase(suspect.first);
                write_membership_list_to_log_file();
            }
        }

        /// Erase the timeout failure machines.
        for (auto &suspect : suspect_erase_pool)
            suspect_list.erase(suspect);

        membership_list_lock.unlock();

        /// Send the failure message of the failed machine to my successors.
        for (auto item : new_failure)
            send_one_entity(item);
    }
}
