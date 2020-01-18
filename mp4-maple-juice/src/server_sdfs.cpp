/**
 * server_sdfs.h
 * Implementation of sdfs funcs in server_sdfs.h.
 */

#include "server_func.h"
#include "general.h"
#include <dirent.h>

#define DEBUG_MODE

/**
 * Send and receive binary files vis TCP.
 * Cited from: https://stackoverflow.com/questions/25634483/send-binary-file-over-tcp-ip-connection
 */
bool senddata(int sock, void *buf, int buflen) {
    auto *pbuf = (unsigned char *) buf;
    while (buflen > 0) {
        int num = send(sock, pbuf, buflen, 0);
        pbuf += num;
        buflen -= num;
    }
    return true;
}

bool sendfile(int sock, FILE *f) {
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    rewind(f);
    if (file_size > 0) {
        char buffer[MAX_BUFFER_SIZE];
        do {
            size_t num = min(file_size, sizeof(buffer));
            num = fread(buffer, 1, num, f);
            if (num < 1) return false;
            if (!senddata(sock, buffer, num)) return false;
            file_size -= num;
        } while (file_size > 0);
    }
    return true;
}

bool readdata(int sock, void *buf, int buf_len) {
    auto pbuf = (unsigned char *) buf;
    while (buf_len > 0) {
        int num = recv(sock, pbuf, buf_len, 0);
        pbuf += num;
        buf_len -= num;
    }
    return true;
}

bool readfile(int sock, FILE *f, size_t file_size) {
    if (file_size > 0) {
        char buffer[MAX_BUFFER_SIZE];
        do {
            int num = min(file_size, sizeof(buffer));
            if (!readdata(sock, buffer, num)) return false;
            int offset = 0;
            do {
                size_t written = fwrite(&buffer[offset], 1, num - offset, f);
                if (written < 1) return false;
                offset += written;
            } while (offset < num);
            file_size -= num;
        } while (file_size > 0);
    }
    return true;
}

void server::init_files_path(string dir_name) {
    dir_name = curr_dir + dir_name;
    DIR *theFolder = opendir(dir_name.c_str());
    if (!theFolder) {
        string command = "mkdir " + dir_name;
        system(command.c_str());
        theFolder = opendir(dir_name.c_str());
    }
    struct dirent *next_file;
    char filepath[512];

    while ((next_file = readdir(theFolder)) != nullptr) {
        /// Build the name for each files in the dir_name.
        sprintf(filepath, "%s/%s", dir_name.c_str(), next_file->d_name);
        remove(filepath);
    }
    closedir(theFolder);
}

int server::hash_string_to_int(const string &input) {
    hash<string> hash_func;
    return (int) (hash_func(input) % NUM_VMS);
}

void server::find_replica_nodes(vector<string> &send_node_list, int file_name_hash) {
//    membership_list_lock.lock();
    if (membership_list.size() <= REPLICA_NUM) {
        while (send_node_list.size() < membership_list.size()) {
            if (membership_list.find(sorted_ips[file_name_hash]) != membership_list.end())
                send_node_list.push_back(sorted_ips[file_name_hash]);
            file_name_hash = (file_name_hash + 1) % NUM_VMS;
        }
    } else {
        while (send_node_list.size() < REPLICA_NUM) {
            if (membership_list.find(sorted_ips[file_name_hash]) != membership_list.end())
                send_node_list.push_back(sorted_ips[file_name_hash]);
            file_name_hash = (file_name_hash + 1) % NUM_VMS;
        }
    }
//    membership_list_lock.unlock();
}

string server::get_file_master_ip(const string &filename) {
    int idx = hash_string_to_int(filename);
    string master_node_ip;
    while (true) {
        if (membership_list.find(sorted_ips[idx]) != membership_list.end()) {
            master_node_ip = sorted_ips[idx];
            break;
        }
        idx = (idx + 1) % NUM_VMS;
    }
    return master_node_ip;
}

void server::handle_put_request(int sock, string put_command) {
    ifstream ifile;
    try {
        /// Check put command, extract local_filename and sdfs_filename.
        stringstream ss(put_command);
        string local_filename, sdfs_filename, command;
        ss >> command >> local_filename >> sdfs_filename;
        if (local_filename.empty() || sdfs_filename.empty())
            throw runtime_error("Command type error\n");
        local_filename = curr_dir + "/files/local/" + local_filename;
        ifile.open(local_filename, ifstream::binary);
        if (!ifile)
            throw runtime_error("Local file not found\n");
        ifile.close();

        /// Obtain the nodes to send the file.
        vector<string> send_node_list;
        int file_name_hash = hash_string_to_int(sdfs_filename);
        find_replica_nodes(send_node_list, file_name_hash);

        /// Check whether the file exists on sdfs
        string target_ip = check_file_exist(sdfs_filename);
        if (target_ip != "-1") {
            bool need_confirm = put_query_check_time_sender(sdfs_filename, target_ip);
            if (need_confirm) {
                bool confirmed = put_query_client_confirm(sock, sdfs_filename);
                if (!confirmed) {
                    close(sock);
                    return;
                }
            }
        }

        /// Prepare sending information: file_size and time_stamp.
        FILE *file = fopen(local_filename.c_str(), "rb");
        fseek(file, 0, SEEK_END);
        uint64_t file_size = ftell(file);
        char *file_buffer = (char *) malloc(sizeof(char) * file_size);
        rewind(file);
        fread(file_buffer, sizeof(char), file_size, file);
        fclose(file);

        std::atomic<int> completed_count{0};

        /// Send to all nodes in the send_node_list.
        for (auto &ip : send_node_list)
            thread(&server::put_query_sender, this, ref(completed_count), local_filename,
                   file_size, sdfs_filename, ip).detach();

        /// For put, wait for QUORUM_W responses.
        while (completed_count < (int) send_node_list.size()) {}

        /// Send back the result of query to client.
        string grep_result = "Put file " + sdfs_filename + " success!\n";
        const char *res = grep_result.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        string empty = "\0";
        const char *res = empty.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
        ifile.close();
    }
}

void server::maple_juice_put(string local_filename, string sdfs_filename) {
    ifstream ifile;
    try {
        if (local_filename.empty() || sdfs_filename.empty())
            throw runtime_error("Command type error\n");
        ifile.open(local_filename, ifstream::binary);
        if (!ifile)
            throw runtime_error("Local file not found\n");
        ifile.close();

        /// Obtain the nodes to send the file.
        vector<string> send_node_list;
        int file_name_hash = hash_string_to_int(sdfs_filename);
        find_replica_nodes(send_node_list, file_name_hash);

        /// Prepare sending information: file_size and time_stamp.
        FILE *file = fopen(local_filename.c_str(), "rb");
        fseek(file, 0, SEEK_END);
        uint64_t file_size = ftell(file);
        char *file_buffer = (char *) malloc(sizeof(char) * file_size);
        rewind(file);
        fread(file_buffer, sizeof(char), file_size, file);
        fclose(file);

        std::atomic<int> completed_count{0};

        /// Send to all nodes in the send_node_list.
        for (auto &ip : send_node_list)
            thread(&server::put_query_sender, this, ref(completed_count), local_filename,
                   file_size, sdfs_filename, ip).detach();

        /// For put, wait for QUORUM_W responses.
        while (completed_count < (int) send_node_list.size()) {}

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        ifile.close();
    }
}

void server::put_query_sender(std::atomic<int> &completed_count, string local_filename, uint64_t file_size,
                              string sdfs_filename, string target_ip) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    ifstream is;
    string response;
#ifdef DEBUG_MODE
    cout << "### Send put query to: " << target_ip << endl;
#endif

    try {
        if (target_ip == my_ip_address) {
            /// If the machine to send file is local machine, then just copy from local dir.
            ostringstream sys_command;
            sys_command << "cp " << local_filename << " " << curr_dir << "/files/sdfs/" << sdfs_filename;
            if (system(sys_command.str().c_str()) != 0)
                throw runtime_error("Bad command\n");
            /// Modify stored_sdfs_files on this node.
            stored_sdfs_files_lock.lock();
            FileInfo newFile{};
            newFile.file_size = file_size;
//            membership_list_lock.lock();
            newFile.is_master = get_file_master_ip(sdfs_filename) == my_ip_address;
//            membership_list_lock.unlock();
            newFile.time_stamp = get_curr_timestamp_milliseconds();
            stored_sdfs_files[sdfs_filename] = newFile;
            stored_sdfs_files_lock.unlock();
        } else {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to target node.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            /// #1 Send: the put_start header: file_name, file_size and time_stamp.
            sdfs_filename = "put_start " + sdfs_filename + " " + to_string(file_size);
            const char *file_name = sdfs_filename.c_str();
            send(sock, file_name, strlen(file_name), 0);

            /// #1 Receive: the get file header success message.
            read(sock, buffer, MAX_BUFFER_SIZE);
            response = buffer;
            if (response != "Get file header success")
                throw runtime_error("Target server get sdfsfilename failed");

            /// #2 Send: the entire file.
            FILE *filehandle = fopen(local_filename.c_str(), "rb");
            sendfile(sock, filehandle);
            fclose(filehandle);

#ifdef DEBUG_MODE
            cout << "### Entire file sent to " << target_ip << endl;
#endif

            /// #2 Receive: the put file success message.
            memset(buffer, 0, sizeof buffer);
            read(sock, buffer, MAX_BUFFER_SIZE);
            response = buffer;

            if (response != "Put file success")
                throw runtime_error("Target server put sdfsfilename failed");

#ifdef DEBUG_MODE
            cout << "### " << target_ip << " received file!" << endl;
#endif
            close(sock);
        }

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }

    /// Increment completed count.
    completed_count++;
}

void server::put_query_receiver(int sock, string query) {
    stringstream ss(query);
    string command, sdfs_filename;
    uint64_t file_size;
    try {
#ifdef DEBUG_MODE
        cout << "### Handling put query..." << endl;
#endif

        /// Check put_start command, extract sdfs_filename, file_size and time_stamp.
        ss >> command >> sdfs_filename >> file_size;
        if (command != "put_start" || sdfs_filename.empty())
            throw runtime_error("Command type error\n");

        /// Modify stored_sdfs_files on this node.
        stored_sdfs_files_lock.lock();
        FileInfo newFile{};
        newFile.file_size = file_size;
//        membership_list_lock.lock();
        newFile.is_master = get_file_master_ip(sdfs_filename) == my_ip_address;
//        membership_list_lock.unlock();
        newFile.time_stamp = get_curr_timestamp_milliseconds();
        stored_sdfs_files[sdfs_filename] = newFile;
        stored_sdfs_files_lock.unlock();

        /// #1 Send: the get file header success message.
        string get_success_command = "Get file header success";
        const char *name_received = get_success_command.c_str();
        send(sock, name_received, strlen(name_received), 0);

        /// #1 Receive: the entire file.
        string file_to_write = curr_dir + "/files/sdfs/" + sdfs_filename;
        FILE *filehandle = fopen(file_to_write.c_str(), "wb");
        readfile(sock, filehandle, file_size);
        fclose(filehandle);

        /// #2 Send: the put file success message.
        string set_success_command = "Put file success";
        const char *file_received = set_success_command.c_str();
        send(sock, file_received, strlen(file_received), 0);
#ifdef DEBUG_MODE
        cout << "### Sent back: " << set_success_command << endl;
#endif

        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
        stored_sdfs_files_lock.lock();
        if (stored_sdfs_files.find(sdfs_filename) != stored_sdfs_files.end())
            stored_sdfs_files.erase(sdfs_filename);
        stored_sdfs_files_lock.unlock();
    }
}

void server::handle_delete_request(int sock, string delete_command) {
    try {
        /// Check delete command, extract local_filename and sdfs_filename.
        stringstream ss(delete_command);
        string sdfs_filename, command, result_message;
        ss >> command >> sdfs_filename;

        /// Obtain the nodes to delete the file.
        vector<string> send_node_list;
        int file_name_hash = hash_string_to_int(sdfs_filename);
        find_replica_nodes(send_node_list, file_name_hash);

        /// First check if this file exists.
        string first_exist_ip = check_file_exist(sdfs_filename);
        if (first_exist_ip == "-1") {
            result_message = "No such file: " + sdfs_filename + "\n";
            const char *res = result_message.c_str();
            send(sock, res, strlen(res), 0);
            close(sock);
            return;
        }

        std::atomic<int> completed_count{0};

        /// Send to all nodes in the send_node_list.
        for (auto &ip : send_node_list)
            thread(&server::delete_query_sender, this, ref(completed_count),
                   sdfs_filename, ip).detach();

        /// For put, wait for QUORUM_W responses.
        while (completed_count < (int) send_node_list.size()) {}

        /// Send back the result of query to client.
        result_message = "Delete file " + sdfs_filename + " success!\n";
        const char *res = result_message.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        string empty = "\0";
        const char *res = empty.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    }
}

void server::delete_query_sender(std::atomic<int> &completed_count, string sdfs_filename, string target_ip) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    ifstream is;
    string response;
    try {
        if (target_ip == my_ip_address) {
            /// If the machine to delete file is local machine, then just delete in my sdfs dir.
            stored_sdfs_files_lock.lock();
            if (stored_sdfs_files.find(sdfs_filename) != stored_sdfs_files.end()) {
                ostringstream sys_command;
                sys_command << "rm " << curr_dir << "/files/sdfs/" << sdfs_filename;
                if (system(sys_command.str().c_str()) != 0)
                    throw runtime_error("Bad command\n");
                /// Modify stored_sdfs_files on this node.
                stored_sdfs_files.erase(sdfs_filename);
            }
            stored_sdfs_files_lock.unlock();
        } else {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to target node.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            /// #1 Send: the delete_start header: file_name, file_size and time_stamp.
            sdfs_filename = "delete_start " + sdfs_filename;
            const char *file_name = sdfs_filename.c_str();
            send(sock, file_name, strlen(file_name), 0);

            /// #1 Receive: the delete result message.
            read(sock, buffer, MAX_BUFFER_SIZE);
            response = buffer;

            if (response == "No such file")
                throw runtime_error("Target server so not have the file");

#ifdef DEBUG_MODE
            cout << "### " << target_ip << " deleted file!" << endl;
#endif
            close(sock);
        }

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }

    /// Increment completed count.
    completed_count++;
}

void server::delete_query_receiver(int sock, string query) {
    stringstream ss(query);
    string command, sdfs_filename;
    try {
#ifdef DEBUG_MODE
        cout << "### Handling delete query..." << endl;
#endif

        /// Check delete_start command, extract sdfs_filename.
        ss >> command >> sdfs_filename;
        if (command != "delete_start" || sdfs_filename.empty())
            throw runtime_error("Command type error\n");

        string send_back_info;
        stored_sdfs_files_lock.lock();
        if (stored_sdfs_files.find(sdfs_filename) == stored_sdfs_files.end()) {
            /// There is no such file in my sdfs.
            send_back_info = "No such file";
        } else {
            stored_sdfs_files.erase(sdfs_filename);
            ostringstream sys_command;
            sys_command << "rm " << curr_dir << "/files/sdfs/" << sdfs_filename;
            if (system(sys_command.str().c_str()) != 0)
                throw runtime_error("Bad command\n");
            send_back_info = "Delete success";
        }
        stored_sdfs_files_lock.unlock();

        /// #1 Send: the get file header success message.
        const char *delete_done_command = send_back_info.c_str();
        send(sock, delete_done_command, strlen(delete_done_command), 0);

#ifdef DEBUG_MODE
        cout << "### Sent back: " << delete_done_command << endl;
#endif

        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }
}

string server::check_file_exist(string sdfs_filename) {
    /// Check whether the file is on the system.
    /// We need to check all replicas to see whether at least one file exist.
#ifdef DEBUG_MODE
    cout << "### Require to check each replica to see whether at least one file exist." << endl;
#endif
    bool none_exist = true;
    string result;
    vector<string> send_node_list;
    int file_name_hash = hash_string_to_int(sdfs_filename);
    find_replica_nodes(send_node_list, file_name_hash);
    for (const auto &ip: send_node_list) {
        int sock = 0;
        char buffer[MAX_BUFFER_SIZE] = {0};
        try {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to server, if fail then mark server as down.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            string ls_file_request = "exist " + sdfs_filename;
            const char *ls_file_c = ls_file_request.c_str();
            send(sock, ls_file_c, strlen(ls_file_c), 0);

            string response;
            /// Read in received message. Clear buffer each time before collect new buffer.
            read(sock, buffer, MAX_BUFFER_SIZE);
            response = buffer;
            if (response == "File exist!") {
                close(sock);
                none_exist = false;
                result = ip;
                break;
            } else if (response == "File doesn't exist!") {
                close(sock);
            } else {
                throw runtime_error("Message connection failed!");
            }
        } catch (runtime_error &e) {
            std::cerr << "error: " << e.what() << std::endl;
            close(sock);
        }
    }
    return none_exist ? "-1" : result;
}

void server::file_exist_request(int sock, string file_exist_command) {
#ifdef DEBUG_MODE
    cout << "### Received file existence check request." << endl;
#endif
    stringstream ss(file_exist_command);
    string sdfs_filename, command;
    ss >> command >> sdfs_filename;

    string response;
    stored_sdfs_files_lock.lock();
    if (stored_sdfs_files.find(sdfs_filename) != stored_sdfs_files.end()) {
        response = "File exist!";
        cout << "File exist!" << endl;
    } else {
        response = "File doesn't exist!";
        cout << "### File doesn't exist!" << endl;
    }
    stored_sdfs_files_lock.unlock();
    const char *res = response.c_str();
    /// Send back the result of query.
    send(sock, res, strlen(res), 0);
    close(sock);
}

void server::handle_ls_request(int sock, string ls_command) {
    stringstream ss(ls_command);
    string sdfs_filename, command;
    ss >> command >> sdfs_filename;

    vector<string> send_node_list;
    int file_name_hash = hash_string_to_int(sdfs_filename);
    find_replica_nodes(send_node_list, file_name_hash);

    string first_exist_ip = check_file_exist(sdfs_filename);
    bool file_exist = !(first_exist_ip == "-1");

    if (!file_exist) {
        /// there's no such file in the system
        string ls_result = "File " + sdfs_filename + " doesn't exist!\n";
        const char *res = ls_result.c_str();
        /// Send back the result of query.
        send(sock, res, strlen(res), 0);
        close(sock);
    } else {
        string ls_result = "Here are the VM addresses of file: " + sdfs_filename + "\n";
        for (const auto &item : send_node_list) {
            string vm_number = sorted_ips_map[item];
            ls_result += "VM" + vm_number + ", ip: " + item + "\n";
        }
        const char *res = ls_result.c_str();
        /// Send back the result of query.
        send(sock, res, strlen(res), 0);
        close(sock);
    }
}

void server::handle_store_request(int sock, string store_command) {
    stored_sdfs_files_lock.lock();

    string store_result = "Here are the SDFS files stored on this machine:\n";

    for (auto &v : stored_sdfs_files) {
        store_result += v.first + "\n";
    }

    const char *res = store_result.c_str();
    send(sock, res, strlen(res), 0);
    close(sock);

    stored_sdfs_files_lock.unlock();
}

void server::handle_get_request(int sock, string get_command) {
    try {
        /// Check get command, extract sdfs_filename.
        stringstream ss(get_command);
        string sdfs_filename, local_filename, command, result_message;
        ss >> command >> sdfs_filename >> local_filename;

        /// Obtain the nodes to get the file.
        vector<string> send_node_list;
        int file_name_hash = hash_string_to_int(sdfs_filename);
        find_replica_nodes(send_node_list, file_name_hash);
        vector<bool> query_status(send_node_list.size(), false);
        mutex query_status_lock;

        /// First check if this file exists.
        string target_get_ip = check_file_exist(sdfs_filename);
        if (target_get_ip == "-1") {
            result_message = "No such file: " + sdfs_filename + "\n";
            const char *res = result_message.c_str();
            send(sock, res, strlen(res), 0);
            close(sock);
            return;
        }

        get_query_sender(sdfs_filename, local_filename, target_get_ip);

        /// Send back the result of query to client.
        result_message = "Get file success! Stored in ./files/fetched/" + local_filename + "\n";
        const char *res = result_message.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        string empty = "\0";
        const char *res = empty.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    }
}

void server::get_query_sender(string sdfs_filename, string local_filename, string target_ip) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    ifstream is;
    string response;
    uint64_t file_size;
#ifdef DEBUG_MODE
    cout << "### Send get query to: " << target_ip << endl;
#endif

    try {
        if (target_ip == my_ip_address) {
            /// If the machine to get file is local machine, then just copy from local dir.
            ostringstream sys_command;
            sys_command << "cp " << curr_dir << "/files/sdfs/" << sdfs_filename << " " << curr_dir
                        << "/files/fetched/" << local_filename;
            if (system(sys_command.str().c_str()) != 0)
                throw runtime_error("Bad command\n");
        } else {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to target node.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            /// #1 Send: the get_start header: file_name.
            sdfs_filename = "get_start " + sdfs_filename;
            const char *file_name = sdfs_filename.c_str();
            send(sock, file_name, strlen(file_name), 0);

            /// #1 Receive: the get file header success message.
            read(sock, buffer, MAX_BUFFER_SIZE);
            stringstream ss(buffer);
            ss >> response >> file_size;
            if (response != "filesize")
                throw runtime_error("Target server failed to receive get command");

            /// #2 Send: get file size success.
            response = "Get file size success";
            const char *response_msg = response.c_str();
            send(sock, response_msg, strlen(response_msg), 0);

            /// #2 Receive: the entire file.
            string file_to_write = curr_dir + "/files/fetched/" + local_filename;
            FILE *filehandle = fopen(file_to_write.c_str(), "wb");
            readfile(sock, filehandle, file_size);
            fclose(filehandle);
#ifdef DEBUG_MODE
            cout << "### Entire file get from " << target_ip << endl;
#endif
            close(sock);
        }

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }
}

void server::get_query_receiver(int sock, string query) {
    stringstream ss(query);
    string command, sdfs_filename, response;
    char buffer[MAX_BUFFER_SIZE] = {0};
    uint64_t file_size;
    try {
#ifdef DEBUG_MODE
        cout << "### Handling get query..." << endl;
#endif

        /// Check put_start command, extract sdfs_filename, file_size and time_stamp.
        ss >> command >> sdfs_filename;
        if (command != "get_start" || sdfs_filename.empty())
            throw runtime_error("Command type error\n");

        /// #1 Send: the file size message.
        /// Prepare sending information: file_size and time_stamp.
        string local_filename = curr_dir + "/files/sdfs/" + sdfs_filename;
        FILE *file = fopen(local_filename.c_str(), "rb");
        fseek(file, 0, SEEK_END);
        file_size = ftell(file);
        char *file_buffer = (char *) malloc(sizeof(char) * file_size);
        rewind(file);
        fread(file_buffer, sizeof(char), file_size, file);
        fclose(file);

        string file_size_command = "filesize " + to_string(file_size);
        const char *response_msg = file_size_command.c_str();
        send(sock, response_msg, strlen(response_msg), 0);

        /// #1 Receive: the get file size success message.
        read(sock, buffer, MAX_BUFFER_SIZE);
        response = buffer;
        if (response != "Get file size success")
            throw runtime_error("Target server get file size failed");

        /// #2 Send: the entire file.
        FILE *filehandle = fopen(local_filename.c_str(), "rb");
        sendfile(sock, filehandle);
        fclose(filehandle);

        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }
}

void server::check_time_valid(int sock, const string &check_time_command) {
    stringstream ss(check_time_command);
    string sdfs_filename, command;
    ss >> command >> sdfs_filename;

    if (stored_sdfs_files.find(sdfs_filename) != stored_sdfs_files.end()) {
        FileInfo curr_file_info = stored_sdfs_files[sdfs_filename];
        uint64_t curr_time = get_curr_timestamp_milliseconds();
        if (curr_time - curr_file_info.time_stamp <= WRITE_WAIT_TIME) {
            string check_result = "Need confirm";
            const char *res = check_result.c_str();
            /// Send back the result of query.
            send(sock, res, strlen(res), 0);
            close(sock);
        } else {
            string check_result = "Don't need confirm";
            const char *res = check_result.c_str();
            /// Send back the result of query.
            send(sock, res, strlen(res), 0);
            close(sock);
        }
    }

}

bool server::put_query_check_time_sender(string sdfs_filename, string target_ip) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    try {
        /// Initialize socket connection.
        struct sockaddr_in serv_addr{};
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            throw runtime_error("Failure in create socket");

        serv_addr.sin_family = AF_INET;

        serv_addr.sin_port = htons(this->sdfs_port);

        if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
            throw runtime_error("Invalid address");

        /// Try to connect to server, if fail then mark server as down.
        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            throw runtime_error("Connection failed");

        string ls_file_request = "check_time " + sdfs_filename;
        const char *ls_file_c = ls_file_request.c_str();
        send(sock, ls_file_c, strlen(ls_file_c), 0);

        string response;
        /// Read in received message. Clear buffer each time before collect new buffer.
        read(sock, buffer, MAX_BUFFER_SIZE);
        response = buffer;
        if (response == "Need confirm") {
            close(sock);
            return true;
        } else if (response == "Don't need confirm") {
            close(sock);
            return false;
        } else {
            throw runtime_error("Message connection failed!");
        }
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        close(sock);
    }
    return false;
}

bool server::put_query_client_confirm(int sock, string sdfs_filename) {
    string check_result = "Need confirm!!";
    const char *res = check_result.c_str();

    /// Send back the result of query.
    send(sock, res, strlen(res), 0);

    char buffer[MAX_BUFFER_SIZE] = {0};
    read(sock, buffer, MAX_BUFFER_SIZE);

    string response = buffer;
    if (response == "Confirm!") {
        return true;
    } else if (response == "Cancelled!") {
        return false;
    } else {
        std::cerr << "error: " << "Message connection failed!" << std::endl;
        return false;
    }
    return false;
}

void server::handle_sdfs_leave_rearrange() {
    /// Update the master for my sdfs files.
    set<string> my_new_master_files;
    for (auto &file : stored_sdfs_files) {
        if (get_file_master_ip(file.first) == my_ip_address) {
            file.second.is_master = true;
            my_new_master_files.insert(file.first);
        }
    }

    /// Store the info for old slaves.
    file_slaves_lock.lock();
    vector<string> prev_slaves = file_slaves;
    refresh_file_slaves();

    /// If I do not have files, do nothing.
    stored_sdfs_files_lock.lock();
    if (stored_sdfs_files.empty()) {
        file_slaves_lock.unlock();
        stored_sdfs_files_lock.unlock();
        return;
    }

    /// Check possible nodes to send files.
    vector<string> send_queue;
    for (const auto &ip : file_slaves)
        if (find(prev_slaves.begin(), prev_slaves.end(), ip) == prev_slaves.end())
            send_queue.push_back(ip);

    std::atomic<int> completed_count{0}, total_task{0};

    uint64_t first_time, last_time;
    first_time = get_curr_timestamp_milliseconds();

    for (const auto &file : stored_sdfs_files) {
        string filename = curr_dir + "/files/sdfs/" + file.first;

        if (!file.second.is_master) continue;

        if (my_new_master_files.find(file.first) != my_new_master_files.end() && membership_list.size() >= 4) {
            total_task++;
#ifdef DEBUG_MODE
            cout << "### Rearrange " << filename << " to " << file_slaves.back() << endl;
#endif
            thread(&server::put_query_sender, this, ref(completed_count), filename,
                   file.second.file_size, file.first, file_slaves.back()).detach();
        } else {
            for (const auto &ip : send_queue) {
                total_task++;
#ifdef DEBUG_MODE
                cout << "### Rearrange " << filename << " to " << ip << endl;
#endif
                thread(&server::put_query_sender, this, ref(completed_count), filename,
                       file.second.file_size, file.first, ip).detach();
            }
        }
    }

    /// Wait for all tasks to complete.
    while (completed_count < total_task) {}

    /// Display time for file transmission.
    if (total_task > 0) {
        last_time = get_curr_timestamp_milliseconds();
#ifdef DEBUG_MODE
        cout << "### Replica cost " << last_time - first_time << "ms" << endl;
#endif
    }
    file_slaves_lock.unlock();
    stored_sdfs_files_lock.unlock();
}

void server::handle_sdfs_join_rearrange() {
    /// Update the master for my sdfs files.
    set<string> my_lose_master_files;
    for (auto &file : stored_sdfs_files) {
        if (file.second.is_master && get_file_master_ip(file.first) != my_ip_address) {
            file.second.is_master = false;
            my_lose_master_files.insert(file.first);
        }
    }

    /// Store the info for old slaves.
    file_slaves_lock.lock();
    vector<string> prev_slaves = file_slaves;
    refresh_file_slaves();

    /// If I do not have files, do nothing.
    stored_sdfs_files_lock.lock();
    if (stored_sdfs_files.empty()) {
        file_slaves_lock.unlock();
        stored_sdfs_files_lock.unlock();
        return;
    }

    /// Check possible nodes to send/delete files.
    vector<string> send_queue, delete_queue;
    for (const auto &ip : file_slaves) {
        if (find(prev_slaves.begin(), prev_slaves.end(), ip) == prev_slaves.end())
            send_queue.push_back(ip);
    }
    for (const auto &ip : prev_slaves)
        if (find(file_slaves.begin(), file_slaves.end(), ip) == file_slaves.end())
            delete_queue.push_back(ip);


    std::atomic<int> completed_count{0}, total_task{0};
    uint64_t first_time, last_time;
    first_time = get_curr_timestamp_milliseconds();

    for (const auto &file : stored_sdfs_files) {
        string filename = curr_dir + "/files/sdfs/" + file.first;
        if (my_lose_master_files.find(file.first) != my_lose_master_files.end()) {
#ifdef DEBUG_MODE
            cout << "### I'm no longer a master for " << file.first << endl;
#endif
            if (membership_list.size() > 4) {
                total_task++;
#ifdef DEBUG_MODE
                cout << "### Delete " << file.first << " from " << file_slaves.back() << endl;
#endif
                thread(&server::delete_query_sender, this, ref(completed_count),
                       file.first, file_slaves.back()).detach();
            }

            total_task++;
#ifdef DEBUG_MODE
            cout << "### Rearrange " << filename << " to " << get_file_master_ip(file.first) << endl;
#endif
            thread(&server::put_query_sender, this, ref(completed_count), filename,
                   file.second.file_size, file.first, get_file_master_ip(file.first)).detach();
        }

        if (!file.second.is_master) continue;

        for (const auto &ip : send_queue) {
            total_task++;
#ifdef DEBUG_MODE
            cout << "### Rearrange " << filename << " to " << ip << endl;
#endif
            thread(&server::put_query_sender, this, ref(completed_count), filename,
                   file.second.file_size, file.first, ip).detach();
        }
        for (const auto &ip : delete_queue) {
            total_task++;
#ifdef DEBUG_MODE
            cout << "### Delete " << file.first << " from " << ip << endl;
#endif
            thread(&server::delete_query_sender, this, ref(completed_count),
                   file.first, ip).detach();
        }
    }

    /// Wait for all tasks to complete.
    while (completed_count < total_task) {}

    /// Display time for file transmission.
    if (total_task > 0) {
        last_time = get_curr_timestamp_milliseconds();
#ifdef DEBUG_MODE
        cout << "### Replica cost " << last_time - first_time << "ms" << endl;
#endif
    }

    file_slaves_lock.unlock();
    stored_sdfs_files_lock.unlock();
}

void server::refresh_file_slaves() {
    file_slaves.clear();
    int pos = find(sorted_ips.begin(), sorted_ips.end(), my_ip_address) - sorted_ips.begin();
    pos = (pos + 1) % NUM_VMS;
    while (file_slaves.size() < 3 && sorted_ips[pos] != my_ip_address) {
        if (membership_list.find(sorted_ips[pos]) != membership_list.end())
            file_slaves.push_back(sorted_ips[pos]);
        pos = (pos + 1) % NUM_VMS;
    }
}

vector<string> server::check_all_exist_file_by_prefix(string prefix) {
    membership_list_lock.lock();
    map<string, Member> curr_membership_list = membership_list;
    membership_list_lock.unlock();
    vector<string> result;
    for (const auto &ip_curr: curr_membership_list) {
        string ip = ip_curr.first;
        int sock = 0;
        char buffer[MAX_BUFFER_SIZE] = {0};
        try {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to server, if fail then mark server as down.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            string ls_file_request = "prefix_exist " + prefix;
            const char *ls_file_c = ls_file_request.c_str();
            send(sock, ls_file_c, strlen(ls_file_c), 0);

            string response;
            /// Read in received message. Clear buffer each time before collect new buffer.
            read(sock, buffer, MAX_BUFFER_SIZE);
            response = buffer;
            string query_type;
            stringstream ss(response);
            ss >> query_type;

            if (query_type == "prefix_exist") {
                close(sock);
                string file_name;
                while (ss >> file_name && !file_name.empty()) {
                    if (find(result.begin(), result.end(), file_name) == result.end()) {
                        result.push_back(file_name);
                    }
                }
            } else if (response == "prefix_not_exist") {
                close(sock);
            } else {
                throw runtime_error("Message connection failed!");
            }
            close(sock);
        } catch (runtime_error &e) {
            std::cerr << "error: " << e.what() << std::endl;
            close(sock);
        }
    }
    cout << "here is the exist prefix files" << endl;
    for (auto &i: result) {
        cout << i << endl;
    }
    return result;
}

void server::delete_all_file_by_prefix(string prefix) {
    membership_list_lock.lock();
    map<string, Member> curr_membership_list = membership_list;
    membership_list_lock.unlock();
    for (const auto &ip_curr: curr_membership_list) {
        string ip = ip_curr.first;
        int sock = 0;
        try {
            /// Initialize socket connection.
            struct sockaddr_in serv_addr{};
            if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                throw runtime_error("Failure in create socket");

            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(this->sdfs_port);

            if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0)
                throw runtime_error("Invalid address");

            /// Try to connect to server, if fail then mark server as down.
            if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
                throw runtime_error("Connection failed");

            cout << "### begin send out delete request!" << endl;

            string ls_file_request = "prefix_delete " + prefix;
            const char *ls_file_c = ls_file_request.c_str();
            send(sock, ls_file_c, strlen(ls_file_c), 0);

            cout << "### send out delete request success!" << endl;
            close(sock);
        } catch (runtime_error &e) {
            std::cerr << "error: " << e.what() << std::endl;
            close(sock);
        }
    }
}

void server::handle_prefix_check_exist(int sock, string file_exist_command) {
    stringstream ss(file_exist_command);
    string prefix, command;
    ss >> command >> prefix;
    string result;
    stored_sdfs_files_lock.lock();
    for (auto &stored_sdfs_file : stored_sdfs_files) {
        if (stored_sdfs_file.first.find(prefix) == 0) {
            result += " " + stored_sdfs_file.first;
        }
    }
    stored_sdfs_files_lock.unlock();
    string curr;
    if (!result.empty()) {
        curr = "prefix_exist" + result;
    } else {
        curr = "prefix_not_exist";
    }
    const char *res = curr.c_str();
    send(sock, res, strlen(res), 0);
    close(sock);
}

void server::handle_prefix_delete(int sock, const string &prefix_delete_command) {
    close(sock);
    stringstream ss(prefix_delete_command);
    string prefix, command;
    ss >> command >> prefix;

    if (prefix.empty()) {
        cout << "### prefix wrong!" << endl;
        return;
    }

    vector<string> erase_list;

    stored_sdfs_files_lock.lock();
    for (auto &stored_sdfs_file : stored_sdfs_files) {
        if (stored_sdfs_file.first.find(prefix) == 0) {
            /// delete the exist file
            erase_list.push_back(stored_sdfs_file.first);
        }
    }

    for (auto &erase_file: erase_list) {
        stored_sdfs_files.erase(erase_file);
        ostringstream sys_command;
        sys_command << "rm " << curr_dir << "/files/sdfs/" << erase_file;
        if (system(sys_command.str().c_str()) != 0)
            throw runtime_error("Bad command\n");
    }
    stored_sdfs_files_lock.unlock();
}
