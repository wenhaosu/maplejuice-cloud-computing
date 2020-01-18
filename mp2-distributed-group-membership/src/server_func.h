/**
 * server_func.cpp
 * Define functions used in server.
 */

#ifndef SERVER_FUNC_H
#define SERVER_FUNC_H

#include "grep.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <cstring>
#include <thread>
#include <stdexcept>
#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <iterator>
#include <algorithm>


#define JOIN 0
#define LEAVE 1
#define HEARTBEAT 2
#define JOIN_SUCCESS 3
#define FAILURE 4
#define ANNOUNCE 5

#define NUM_HEARTBEAT_LISTEN_TARGET 3
#define NUM_HEARTBEAT_SEND_TARGET 3
#define HEARTBEAT_WAIT_MILLISECONDS 500
#define FAILURE_SUSPECT_WAIT_MILLISECONDS 3000
#define TIMEOUT_ERASE_MILLISECONDS 4000

#define LOG_FILE_PATH "mp2.log"

struct Member {
    char ip_address[14];
    uint64_t time_stamp;
    uint64_t updated_time;

    bool operator<(Member other) const {
        return string(ip_address) < string(other.ip_address);
    }
};


class server {
private:
    /// The port that server is listening.
    int port_num;

    /// The ip address of current machine.
    string my_ip_address;

    /// The ip address of indicator machine.
    const string indicator_ip = "172.22.154.5";

    /// A boolean parameter indicating whether current machine has joined the group.
    bool is_joined;

    /// The sorted hash table for storing the membership list of the joined group.
    map<string, Member> membership_list;
    map<string, Member> suspect_list;
    mutex membership_list_lock;

    /// The lock for writing log files.
    mutex log_file_lock;

    /// The last time of sending a heartbeat to other machines.
    uint64_t last_send_heartbeat_time;

    /**
     * Thread for receiving TCP messages from the listening port.
     */
    void run_tcp_receiver();

    /**
     * Thread for receiving UDP messages from the listening port.
     */
    void run_udp_receiver();

    /**
     * Thread for handle connection from client.
     *
     * Parameters:
     *      sock: The socket parameter for connection.
     */
    int handle_grep_request(int sock);

    /**
     * Update the current membership list according to received list from other machine.
     *
     * Parameters:
     *      received_list: Received membership list in vector format.
     */
    void update_membership(vector<Member> received_list);

    /**
     * Decode the received membership list from raw char* and convert network byte order to host order.
     *
     * Parameters:
     *      raw_data: Received membership list in char* format.
     *      num_bytes: Number of bytes received.
     *
     * Returns:
     *      Return a vector of decoded membership list in host byte order.
     */
    static vector<Member> decode_membership_list(char *raw_data, int num_bytes);

    /**
     * Encode the received membership list before sending, convert host byte order to network order.
     *
     * Parameters:
     *      message: Membership list to send in host byte order.
     *
     * Returns:
     *      Return the vector of membership list in network byte order.
     */
    static vector<Member> encode_membership_list(vector<Member> message);

    /**
     * Convert and return the current membership list from map to vector.
     *
     * Returns:
     *      Return the current membership list in vector format.
     */
    vector<Member> get_membership_list_as_vector();

    /**
     * Get the current timestamp on this machine in milliseconds from epoch.
     *
     * Returns:
     *      Return the current timestamp in uint64_t format.
     */
    static uint64_t get_curr_timestamp_milliseconds();

    /**
     * Return the IP address of current machine.
     * Cited from: https://gist.github.com/quietcricket/2521037
     *
     * Returns:
     *      Return the ip address of current machine in string format.
     */
    static string get_my_ip_address();

    /**
     * Print the input hint message in console user interface.
     */
    void console_message();

    /**
     * Handle "join" command from user input.
     */
    void handle_join_group();

    /**
     * Handle "leave" command from user input.
     */
    void handle_leave_group();

    /**
     * Handle send failure detected message to target members
     */
    void send_one_entity(Member failed_member);

    /**
     * Send udp message to given target host.
     *
     * Parameters:
     *      target_ip_addr: The ip address of target machine in string format.
     *      messages: A vector of messages to send in Member format.
     */
    void run_udp_sender(string target_ip_addr, vector<Member> messages);

    /**
     * Handle sending heartbeat messages periodically.
     */
    void heartbeat_sender();

    /**
     * Print the current membership list to terminal.
     */
    void print_curr_membership_list();

    /**
     * Return the send target of current server.
     */
    vector<string> find_my_send_targets();

    /**
     * Return the listen target of current server.
     */
    vector<string> find_my_listen_targets();

    /**
     * Write message to the log file.
     */
    void write_log_file(string log_message, int type);

    /**
     * Detect whether a listen target has failed.
     */
    void failure_detector();

    /**
     * Print the current membership list to log file.
     */
    void write_membership_list_to_log_file();

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
