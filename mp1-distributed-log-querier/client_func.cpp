/**
 * client_func.cpp
 * Implementation of functions in client_func.h.
 */

#include "client_func.h"

/// The maximum size of single buffer.
#define MAX_BUFFER_SIZE 2000000

/// Uncomment this line to enter debug mode.
//#define DEBUG_MODE

client::client(int port_num) {
    this->port_num = port_num;
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


int client::run_client() {

#ifdef DEBUG_MODE
    cout << "Running client..." << endl;
#endif

    string input;

    /// Create the thread for checking and processing queries.
    auto processing_thread = thread(&client::process_queries, this);
    processing_thread.detach();

#ifdef DEBUG_MODE
    cout << "Processing thread detached..." << endl;
#endif

    while (getline(std::cin, input)) {
        /// User can end client by typing exit.
        if (input == "exit") return 0;
        if (input.empty()) continue;

        /// Push the query message to a queue.
        queries_lock.lock();
        queries.push(input);
        queries_lock.unlock();
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
        serv_addr.sin_port = htons(this->port_num);

#ifdef DEBUG_MODE
        cout << "Try connecting to " << ip_addresses[vm_id] << " with vm id " << vm_id << endl;
#endif

        if (inet_pton(AF_INET, ip_addresses[vm_id].c_str(), &serv_addr.sin_addr) <= 0)
            throw runtime_error("Invalid address");

        /// Try to connect to server, if fail then mark server as down.
        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            throw runtime_error("Connection failed");

        const char *query = input.c_str();
        send(sock, query, strlen(query), 0);

#ifdef DEBUG_MODE
        printf("Grep message sent: %s\n", query);
#endif

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

#ifdef DEBUG_MODE
        cout <<"Remain "<<queries.size() << " queries to process"<< endl;
#endif
    }
}
