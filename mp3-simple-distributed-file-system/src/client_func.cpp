/**
 * client_func.cpp
 * Implementation of functions in client_func.h.
 */

#include "client_func.h"
#include "general.h"

/// Uncomment this line to enter debug mode.
//#define DEBUG_MODE

client::client() {
    this->query_port = 8000;
    this->my_ip_address = get_my_ip_address();
    ip_status.resize(ip_addresses.size(), true);
    query_status.resize(MAX_VM_NUM, false);
}

void delete_all_temp_files() {
    /// Temp files are named "out_*.txt".
    for (unsigned long vm_id = 0; vm_id < MAX_VM_NUM; vm_id++) {
        string file_name = "out_";
        file_name += (to_string(vm_id) + ".out");
        remove(file_name.c_str());
    }
}

void client::console_message() {
    cout << ">>> Please enter the following command:" << endl;
    cout << "=======================================" << endl;
    cout << "grep command" << endl;
    cout << "put localfilename sdfsfilename" << endl;
    cout << "get sdfsfilename localfilename" << endl;
    cout << "delete sdfsfilename" << endl;
    cout << "ls sdfsfilename" << endl;
    cout << "store" << endl;
    cout << "help" << endl;
    cout << "exit" << endl;
    cout << "=======================================" << endl;
}

int client::run_client() {

    cout << "Running client..." << endl;
    string input, command;

    /// Create the thread for checking and processing queries.
    auto processing_thread = thread(&client::process_queries, this);
    processing_thread.detach();

    console_message();
    while (getline(std::cin, input)) {
        if (wait_for_confirm) {
            string choice;
            if (input.empty()) continue;
            stringstream ss(input);
            ss >> choice;
            string final_choice;
            if (choice == "yes") {
                final_choice = "yes";
            } else {
                final_choice = "no";
            }
            send_put_query_second_part(final_choice);
            wait_for_confirm = false;
            wait_for_confirm_sock = 0;
        } else {
            /// User can end client by typing exit.
            if (input.empty()) continue;
            stringstream ss(input);
            ss >> command;
            if (command == "exit") return 0;
            else if (command == "grep") {
                /// Push the query message to a queue.
                queries_lock.lock();
                queries.push(input);
                queries_lock.unlock();
            } else if (command == "put") {
                string sdfs_filename, local_filename;
                ss >> local_filename >> sdfs_filename;
                if (local_filename.empty() || sdfs_filename.empty()) {
                    cout << "Please enter the right command!" << endl;
                } else {
                    send_put_query(input);
                }
            } else if (command == "get") {
                string sdfs_filename, local_filename;
                ss >> sdfs_filename >> local_filename;
                if (local_filename.empty() || sdfs_filename.empty()) {
                    cout << "Please enter the right command!" << endl;
                } else {
                    send_sdfs_query(input);
                }
            } else if (command == "delete") {
                string sdfs_filename;
                ss >> sdfs_filename;
                if (sdfs_filename.empty()) {
                    cout << "Please enter the right command!" << endl;
                } else {
                    send_sdfs_query(input);
                }
            } else if (command == "ls") {
                string sdfs_filename;
                ss >> sdfs_filename;
                if (sdfs_filename.empty()) {
                    cout << "Please enter the right command!" << endl;
                } else {
                    send_sdfs_query(input);
                }
            } else if (command == "store") {
                send_sdfs_query(input);
            } else if (command == "help") {
                console_message();
            } else {
                cout << "Please enter the right command!" << endl;
            }
        }
    }

    return 0;
}

void client::wait_30_seconds() {
    uint64_t begin_time = get_curr_timestamp_milliseconds();
    while (wait_for_confirm && get_curr_timestamp_milliseconds() - begin_time < 30000) {

    }

    if (wait_for_confirm) {
        string over_write_response = "Cancelled!";
        const char *res = over_write_response.c_str();
        send(wait_for_confirm_sock, res, strlen(res), 0);
        cout << "Time limit exceeds, write is cancelled." << endl;
        wait_for_confirm = false;
        wait_for_confirm_sock = 0;
    }

}

uint64_t client::get_curr_timestamp_milliseconds() {
    return (uint64_t) chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
}

void client::send_put_query_second_part(string choice) {
    //TODO: create a thread to calculate time
    bool over_write = false;
    char buffer[MAX_BUFFER_SIZE] = {0};
    if (choice == "yes") {
        over_write = true;
    } else if (choice == "no") {
        over_write = false;
    } else {
        cout << "Invalid response, write is cancelled!" << endl;
        over_write = false;
    }

    if (over_write) {
        cout << "Write is confirmed." << endl;
        string over_write_response = "Confirm!";
        const char *res = over_write_response.c_str();
        send(wait_for_confirm_sock, res, strlen(res), 0);

        while (read(wait_for_confirm_sock, buffer, MAX_BUFFER_SIZE)) {
            cout << buffer << endl;
        }
    } else {
        cout << "Write is cancelled." << endl;
        string over_write_response = "Cancelled!";
        const char *res = over_write_response.c_str();
        send(wait_for_confirm_sock, res, strlen(res), 0);
    }
}

int client::send_put_query(string input) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};

    stringstream ss(input);
    string sdfs_filename, local_filename, command;
    ss >> command >> local_filename >> sdfs_filename;

    /// Initialize socket connection.
    struct sockaddr_in serv_addr{};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw runtime_error("Failure in create socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(this->query_port);

    if (inet_pton(AF_INET, my_ip_address.c_str(), &serv_addr.sin_addr) <= 0)
        throw runtime_error("Invalid address");

    /// Try to connect to the server of the same ip address and print the result.
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw runtime_error("Connection failed");

    const char *query = input.c_str();
    send(sock, query, strlen(query), 0);

    read(sock, buffer, MAX_BUFFER_SIZE);

    string response = buffer;

    if (response == "Need confirm!!") {
        wait_for_confirm = true;
        wait_for_confirm_sock = sock;
        thread(&client::wait_30_seconds, this).detach();
        cout << "Please confirm write to file: " + sdfs_filename + " [yes/no]" << endl;
    } else {
        cout << buffer << endl;
    }

    return 0;
}

int client::send_sdfs_query(string input) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};

    /// Initialize socket connection.
    struct sockaddr_in serv_addr{};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        throw runtime_error("Failure in create socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(this->query_port);

    if (inet_pton(AF_INET, my_ip_address.c_str(), &serv_addr.sin_addr) <= 0)
        throw runtime_error("Invalid address");

    /// Try to connect to the server of the same ip address and print the result.
    if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        throw runtime_error("Connection failed");

    const char *query = input.c_str();
    send(sock, query, strlen(query), 0);

    while (read(sock, buffer, MAX_BUFFER_SIZE)) {
        cout << buffer << endl;
    }

    return 0;
}


int client::send_grep_query(int vm_id, string input) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    try {
        /// Initialize socket connection.
        struct sockaddr_in serv_addr{};
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            throw runtime_error("Failure in create socket");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(this->query_port);

        if (inet_pton(AF_INET, ip_addresses[vm_id].c_str(), &serv_addr.sin_addr) <= 0)
            throw runtime_error("Invalid address");

        /// Try to connect to server, if fail then mark server as down.
        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            throw runtime_error("Connection failed");

        const char *query = input.c_str();
        send(sock, query, strlen(query), 0);

        write_result.lock();
        /// Create temp files to store query result from different servers.
        string output_file_name = "out_";
        output_file_name += (to_string(vm_id) + ".out");
        ofstream outfile(output_file_name);

        /// Read in received message. Clear buffer each time before collect new buffer.
        while (read(sock, buffer, MAX_BUFFER_SIZE)) {
            outfile << buffer;
            memset(buffer, 0, sizeof buffer);
        }

        /// Change query status to finished status.
        query_status_lock.lock();
        query_status[vm_id] = true;
        outfile.close();
        query_status_lock.unlock();
        write_result.unlock();

    } catch (runtime_error &e) {
        if (strcmp(e.what(), "Connection failed") == 0) {
            /// If connection failed, mark server as down.
            ip_status_lock.lock();
            cout_lock.lock();
            ip_status[vm_id] = false;
            cout << "@@@ Connection failed with vm " << vm_id + 1 << endl;
            cout_lock.unlock();
            ip_status_lock.unlock();
        } else {
            std::cerr << "error: " << e.what() << std::endl;
        }
        close(sock);
        return -1;
    }
    return 0;
}

void client::process_queries() {
    while (true) {
        /// Continue if there are no queries in the queue.
        if (queries.empty()) continue;

        /// Grab and pop the first query the queue.
        queries_lock.lock();
        string curr_query = queries.front();
        queries.pop();
        queries_lock.unlock();

        delete_all_temp_files();

        /// Assign tasks to all alive serves.
        ip_status_lock.lock();
        for (unsigned long vm_id = 0; vm_id < MAX_VM_NUM; vm_id++)
            if (ip_status[vm_id])
                thread(&client::send_grep_query, this, vm_id, curr_query).detach();
        ip_status_lock.unlock();

        /// Begin checking whether all queries are finished.
        bool is_complete = false;
        while (!is_complete) {
            is_complete = true;
            query_status_lock.lock();

            /// A query is finished if received & alive or not received & died.
            for (unsigned long vm_id = 0; vm_id < MAX_VM_NUM && is_complete; vm_id++)
                is_complete = !(ip_status[vm_id] ^ query_status[vm_id]);

            query_status_lock.unlock();
        }

        cout_lock.lock();
        query_status_lock.lock();

        /// Combine all results from different servers into one file.
        string outfile_name = "result.out";
        remove(outfile_name.c_str());
        ofstream outfile(outfile_name);

        cout << ">>>>> Result for command " << curr_query << " :" << endl;

        for (unsigned long vm_id = 0; vm_id < MAX_VM_NUM; vm_id++)
            if (query_status[vm_id]) {
                /// Append individual results to result.out, and print returned line numbers to terminal.
                string infile_name = "out_";
                infile_name += (to_string(vm_id) + ".out");
                ifstream infile(infile_name);
                int lines = count(istreambuf_iterator<char>(infile), istreambuf_iterator<char>(), '\n');
                string flag = lines <= 1 ? " line." : " lines.";
                cout << "From vm" << vm_id + 1 << ": " << lines << flag << endl;
                if (lines != 0) {
                    infile.seekg(0, ios::beg);
                    outfile << ">>>>> Result from vm " << vm_id + 1 << " :" << endl << infile.rdbuf();
                    infile.close();
                }
            }
        outfile.close();

        delete_all_temp_files();

        cout << ">>>>> Result for command " << curr_query << " printed." << endl;
        cout << ">>>>> Detailed result stored in result.out file." << endl;
        cout_lock.unlock();

        /// Reset query status vector to be all false.
        fill(query_status.begin(), query_status.end(), false);
        query_status_lock.unlock();
    }
}
