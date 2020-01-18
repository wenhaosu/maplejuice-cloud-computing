/**
 * server_func.h
 * Define functions used in server.
 */

#ifndef SERVER_FUNC_H
#define SERVER_FUNC_H

#include "grep.h"
#include "server_membership.h"
#include "server_maplejuice.h"
#include "server_sdfs.h"
#include "general.h"
#include <cstring>
#include <atomic>
#include <thread>
#include <stdexcept>
#include <map>
#include <queue>
#include <set>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <iterator>
#include <algorithm>


class server {
private:
    /// The port that server is listening.
    int query_port;
    int hb_port;
    int sdfs_port;
    int mj_port;

    /// The ip address of current machine.
    string my_ip_address;

    /// The ip address of indicator machine.
    const string indicator_ip = "172.22.154.5";

    /// The current directory that the program is running.
    string curr_dir;

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

    /// Hash table holding all files stored on sdfs of this machine.
    unordered_map<string, FileInfo> stored_sdfs_files;
    mutex stored_sdfs_files_lock;

    /// The file slaves of this node.
    vector<string> file_slaves;
    mutex file_slaves_lock;

    /// List of all possible machines, sorted in string order.
    const vector<string> sorted_ips = {
            "172.22.152.11",
            "172.22.152.12",
            "172.22.152.6",
            "172.22.154.5",
            "172.22.154.6",
            "172.22.154.7",
            "172.22.154.8",
            "172.22.156.5",
            "172.22.156.6",
            "172.22.156.7"
    };

    /// The maple juice queue received from client.
    queue<pair<string, int>> maple_juice_requests;
    mutex maple_juice_requests_lock;

    /// The job done count for maple or juice stage.
    int maple_juice_done_count;
    mutex maple_juice_done_count_lock;

    /// The .
    queue<string> free_worker;
    mutex free_worker_lock;

    /// Hash table to transfer ips to VM nums.
    unordered_map<string, string> sorted_ips_map = {
            {"172.22.152.11", "6"},
            {"172.22.152.12", "9"},
            {"172.22.152.6",  "3"},
            {"172.22.154.5",  "1"},
            {"172.22.154.6",  "4"},
            {"172.22.154.7",  "7"},
            {"172.22.154.8",  "10"},
            {"172.22.156.5",  "2"},
            {"172.22.156.6",  "5"},
            {"172.22.156.7",  "8"}
    };

    /**
     * Thread for receiving TCP messages from the listening port.
     */
    void run_tcp_query_receiver();

    void run_tcp_sdfs_receiver();

    /**
     * Thread for receiving UDP messages from the listening port.
     */
    void run_udp_receiver();

    /**
     * Thread for handle grep request from client.
     */
    int handle_grep_request(int sock, string grep_command);

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

    /**
     * Delete all files in the given path.
     * Cited from: https://stackoverflow.com/questions/11007494/how-to-delete-all-files-in-a-folder-but-not-delete-the-folder-using-nix-standar
     */
    void init_files_path(string dir_name);

    static int hash_string_to_int(const string &input);

    /**
     * Thread for handle put request from client.
     */
    void handle_put_request(int sock, string put_command);

    /**
     * Thread for handle get request from client.
     */
    void handle_get_request(int sock, string get_command);

    /**
     * Thread for handle delete request from client.
     */
    void handle_delete_request(int sock, string delete_command);

    /**
     * Thread for handle ls request from client.
     */
    void handle_ls_request(int sock, string ls_command);

    /**
     * Thread for handle store request from client.
     */
    void handle_store_request(int sock, string store_command);

    /**
     * Find all the nodes that needs to have a replica of the file according to given file hash.
     */
    void find_replica_nodes(vector<string> &send_node_list, int file_name_hash);

    /**
     * Thread for sending a put query.
     */
    void put_query_sender(std::atomic<int> &completed_count, string local_filename,
                          uint64_t file_size, string sdfs_filename, string target_ip);

    /**
     * Thread for receiving a put query.
     */
    void put_query_receiver(int sock, string query);

    /**
     * Thread for sending a delete query.
     */
    void delete_query_sender(std::atomic<int> &completed_count, string sdfs_filename, string target_ip);

    /**
     * Thread for receiving a delete query.
     */
    void delete_query_receiver(int sock, string query);

    /**
     * Thread for sending a get query.
     */
    void get_query_sender(string sdfs_filename, string local_filename, string target_ip);

    /**
     * Thread for receiving a get query.
     */
    void get_query_receiver(int sock, string query);

    /**
     * Thread for sending a request to find whether a file exists in sdfs.
     */
    void file_exist_request(int sock, string file_exist_command);

    /**
     * Check whether a sdfs file exists in the sdfs.
     *
     * Returns:
     *      Return the master node ip address of the file, otherwise return "-1".
     */
    string check_file_exist(string sdfs_filename);

    /**
     * Get the master node of a given sdfs file.
     */
    string get_file_master_ip(const string &filename);

    bool put_query_check_time_sender(string sdfs_filename, string target_ip);

    bool put_query_client_confirm(int sock, string sdfs_filename);

    void check_time_valid(int sock, const string &check_time_command);

    /**
     * Refresh my file slaves.
     */
    void refresh_file_slaves();

    /**
     * Rearrange sdfs files if a leave occurs.
     */
    void handle_sdfs_leave_rearrange();

    /**
     * Rearrange sdfs files if a join occurs.
     */
    void handle_sdfs_join_rearrange();

    /**
     * Handle "maple/juice" command from user input.
     */
    void run_maple_juice_handler();

    /**
     * Handle the maple query from user, should only be called by master node.
     */
    void handle_maple_query(pair<string, int> query);

    /**
     * Handle the juice query from user, should only be called by master node.
     */
    void handle_juice_query(pair<string, int> query);

    /**
     * Get the filenames on sdfs which starts by prefix.
     *
     * Returns:
     *      Return the vector of all sdfs filenames starting by prefix.
     */
    vector<string> check_all_exist_file_by_prefix(string prefix);

    /**
     * Handle the prefix existence check query.
     */
    void handle_prefix_check_exist(int sock, string prefix_exist_command);

    /**
     * Delete all files on sdfs starting by prefix.
     */
    void delete_all_file_by_prefix(string prefix);

    /**
     * Handle the prefix delete query.
     */
    void handle_prefix_delete(int sock, const string &prefix_delete_command);

    /**
     * Assign maple jobs to nodes using range based strategy.
     */
    void range_based_assign(map<string, MapleMission> &worker_mission_pair,
                            map<string, Member> &curr_membership_list,
                            vector<string> &sdfs_source_files);

    /**
     * Assign maple jobs to nodes using hash based strategy.
     */
    void hash_based_assign(map<string, MapleMission> &worker_mission_pair,
                           map<string, Member> &curr_membership_list,
                           vector<string> &sdfs_source_files);

    /**
     * Monitor the maple task from a slave, should only be called by master node.
     */
    void maple_task_monitor(MapleMission &mission, string target_ip, string maple_exe, string sdfs_prefix);

    /**
     * Process a maple job, should only be called by slave node.
     */
    void maple_task_processor(int sock, string process_command);

    /**
     * Monitor the juice task from a slave, should only be called by master node.
     */
    void
    juice_task_monitor(JuiceMission &mission, string target_ip, string juice_exe, string sdfs_dest, int delete_input);

    /**
     * Process a juice job, should only be called by slave node.
     */
    void juice_task_processor(int sock, string process_command);

    /**
     * Receive maple juice requests, should only be called by slave node.
     */
    void run_tcp_maple_juice_receiver();

    /**
     * Internal put protocol running by slaves/master in maple juce.
     */
    void maple_juice_put(string local_filename, string sdfs_filename);

public:

    /**
     * Initializer for the server class.
     *
     * Parameters:
     *      port_num: The port that server is listening.
     */
    explicit server();

    /**
     * Run the initialized server.
     */
    int run_server();
};

#endif //SERVER_FUNC_H
