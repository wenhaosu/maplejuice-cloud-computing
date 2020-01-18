/**
 * server_maplejuice.h
 * Implementation of maplejuice funcs in server_func.h.
 */

#include "server_func.h"
#include "server_maplejuice.h"
#include "general.h"

#define TEN_LINES_READ false
#define USE_RANGE_BASED_PARTITION true

void server::run_maple_juice_handler() {
    while (true) {
        maple_juice_requests_lock.lock();
        if (maple_juice_requests.empty()) {
            maple_juice_requests_lock.unlock();
            continue;
        }
        pair<string, int> query = maple_juice_requests.front();
        maple_juice_requests.pop();
        maple_juice_requests_lock.unlock();
        string query_type;
        stringstream ss(query.first);
        ss >> query_type;
        if (query_type == "maple") handle_maple_query(query);
        else handle_juice_query(query);

    }
}

void server::handle_maple_query(pair<string, int> query) {
    string command = query.first, phase, maple_exe, sdfs_prefix, sdfs_src;
    cout << "### Receive maple query:" << command << endl;
    int sock = query.second, num_maples = 0, available_workers = 0;
    map<string, Member> curr_membership_list;
    vector<string> sdfs_source_files;

    map<string, MapleMission> worker_mission_pair;

    while (!free_worker.empty()) free_worker.pop();

    maple_juice_done_count = 0;

    try {

        /// Decode maple command.
        stringstream ss(command);
        ss >> phase >> maple_exe >> num_maples >> sdfs_prefix >> sdfs_src;

        /// Conduct error handling.
        if (phase != "maple")
            throw runtime_error("Command type error!");

        cout << "begin membership to curr membership list" << endl;

        membership_list_lock.lock();
        if ((int) membership_list.size() <= num_maples + 1) {
            cout << "simple copy the membership list " << endl;
            curr_membership_list = membership_list;
            curr_membership_list.erase(my_ip_address);
        } else {
            cout << "begin choose the first num of members" << endl;
            auto it = membership_list.begin();
            while ((int) curr_membership_list.size() < num_maples) {
                cout << it->first << endl;
                if (it->first != my_ip_address) {
                    curr_membership_list[it->first] = it->second;
                }
                it++;
                cout << curr_membership_list.size() << endl;
            }
        }
        membership_list_lock.unlock();

        available_workers = (int) curr_membership_list.size();

        if (available_workers <= 0)
            throw runtime_error("No enough workers!");

        if (check_file_exist(maple_exe) == "-1")
            throw runtime_error("No such maple_exe, please first put it onto sdfs!");

        sdfs_source_files = check_all_exist_file_by_prefix(sdfs_src);
        if (sdfs_source_files.empty())
            throw runtime_error("No such sdfs intermediate filename prefix!");

        /// Use selected partition strategy to assign files.
        if (USE_RANGE_BASED_PARTITION)
            range_based_assign(worker_mission_pair, curr_membership_list, sdfs_source_files);
        else
            hash_based_assign(worker_mission_pair, curr_membership_list, sdfs_source_files);

        cout << "### Maple workers assigned:" << endl;
        for (const auto &item : worker_mission_pair) {
            cout << "### " << item.first << ": (" << item.second.mission_id << ") ";
            for (const auto &file : item.second.files) cout << file << " ";
            cout << endl;
        }

        /// Assign maple missions to selected slaves.
        for (auto &item: worker_mission_pair) {
            thread(&server::maple_task_monitor, this, ref(item.second), item.first, maple_exe, sdfs_prefix).detach();
        }

        /// Wait for maple missions to all finish.
        while (true) {
            maple_juice_done_count_lock.lock();
            if (maple_juice_done_count == (int) worker_mission_pair.size()) {
                maple_juice_done_count_lock.unlock();
                break;
            }
            maple_juice_done_count_lock.unlock();
        }

        string over_write_response = "Maple job: (" + command + ") finished!";
        const char *res = over_write_response.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        string error = e.what();
        const char *res = error.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    }

}

void server::handle_juice_query(pair<string, int> query) {
    string command = query.first, phase, juice_exe, sdfs_prefix, sdfs_dest;
    int delete_input = 0;
    cout << "### Receive juice query:" << command << endl;

    int sock = query.second, num_juices = 0, available_workers = 0;
    map<string, Member> curr_membership_list;
    vector<string> sdfs_source_files;

    map<string, JuiceMission> worker_mission_pair;

    while (!free_worker.empty()) free_worker.pop();

    maple_juice_done_count = 0;

    try {

        /// Decode juice command.
        stringstream ss(command);
        ss >> phase >> juice_exe >> num_juices >> sdfs_prefix >> sdfs_dest >> delete_input;

        /// Conduct error handling.
        if (phase != "juice")
            throw runtime_error("Command type error!");

        cout << "begin membership to curr membership list" << endl;

        membership_list_lock.lock();
        if ((int) membership_list.size() <= num_juices + 1) {
            cout << "simple copy the membership list " << endl;
            curr_membership_list = membership_list;
            curr_membership_list.erase(my_ip_address);
        } else {
            cout << "begin choose the first num of members" << endl;
            auto it = membership_list.begin();
            while ((int) curr_membership_list.size() < num_juices) {
                cout << it->first << endl;
                if (it->first != my_ip_address) {
                    curr_membership_list[it->first] = it->second;
                }
                it++;
                cout << curr_membership_list.size() << endl;
            }
        }
        membership_list_lock.unlock();

        available_workers = (int) curr_membership_list.size();

        if (available_workers <= 0)
            throw runtime_error("No enough workers!");

        if (check_file_exist(juice_exe) == "-1")
            throw runtime_error("No such juice_exe, please first put it onto sdfs!");

        sdfs_source_files = check_all_exist_file_by_prefix(sdfs_prefix);
        if (sdfs_source_files.empty())
            throw runtime_error("No such sdfs intermediate filename prefix!");


        /// Use range based partition strategy to assign files.
        vector<string> membership;
        membership.reserve(curr_membership_list.size());
        for (const auto &item : curr_membership_list) membership.push_back(item.first);
        int mission_id = 0;
        int cnt = 0;
        for (int i = 0; i < NUM_VMS; i++) {
            if (worker_mission_pair.find(membership[cnt]) == worker_mission_pair.end()) {
                JuiceMission new_mission = {mission_id++, PHASE_I, {sdfs_prefix + "_" + to_string(i)}};
                worker_mission_pair[membership[cnt]] = new_mission;
            } else {
                worker_mission_pair[membership[cnt]].prefixes.push_back(sdfs_prefix + "_" + to_string(i));
            }
            cnt = (cnt + 1) % (int) membership.size();
        }

        cout << "### Juice workers assigned:" << endl;
        for (const auto &item : worker_mission_pair) {
            cout << "### " << item.first << ": (" << item.second.mission_id << ") ";
            for (const auto &file : item.second.prefixes) cout << file << " ";
            cout << endl;
        }

        /// Assign juice missions to selected slaves.
        for (auto &item: worker_mission_pair) {
            thread(&server::juice_task_monitor, this, ref(item.second), item.first, juice_exe, sdfs_dest,
                   delete_input).detach();
        }

        /// Wait for juice missions to all finish.
        while (true) {
            maple_juice_done_count_lock.lock();
            if (maple_juice_done_count == (int) worker_mission_pair.size()) {
                maple_juice_done_count_lock.unlock();
                break;
            }
            maple_juice_done_count_lock.unlock();
        }

        /// Combine the final juice outputs from multiple slaves to a single file on master node, and put to sdfs.
        for (int i = 0; i < mission_id; i++) {
            string juice_output_file_name = sdfs_dest + "_" + to_string(i);
            string target_get_ip = check_file_exist(juice_output_file_name);
            get_query_sender(juice_output_file_name, juice_output_file_name, target_get_ip);
        }

        string sys_command = "cat files/fetched/" + sdfs_dest + "* | sort > files/fetched/" + sdfs_dest;
        system(sys_command.c_str());
        maple_juice_put(curr_dir + "/files/fetched/" + sdfs_dest, sdfs_dest);
        sys_command = "rm files/fetched/" + sdfs_dest + "*";
        system(sys_command.c_str());

        for (int i = 0; i < mission_id; i++) {
            string juice_output_file_name = sdfs_dest + "_" + to_string(i);
            delete_all_file_by_prefix(juice_output_file_name);
        }

        string over_write_response = "Juice job: (" + command + ") finished!";
        const char *res = over_write_response.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        string error = e.what();
        const char *res = error.c_str();
        send(sock, res, strlen(res), 0);
        close(sock);
    }
}


void server::range_based_assign(map<string, MapleMission> &worker_mission_pair,
                                map<string, Member> &curr_membership_list,
                                vector<string> &sdfs_source_files) {
    vector<string> membership;
    membership.reserve(curr_membership_list.size());
    for (const auto &item : curr_membership_list) membership.push_back(item.first);
    int cnt = 0;
    int mission_id = 0;
    for (const auto &file : sdfs_source_files) {
        if (worker_mission_pair.find(membership[cnt]) == worker_mission_pair.end()) {
            MapleMission new_mission = {mission_id++, PHASE_I, {file}};
            worker_mission_pair[membership[cnt]] = new_mission;
        } else {
            worker_mission_pair[membership[cnt]].files.push_back(file);
        }

        cnt = (cnt + 1) % (int) membership.size();
    }
}

void server::hash_based_assign(map<string, MapleMission> &worker_mission_pair,
                               map<string, Member> &curr_membership_list,
                               vector<string> &sdfs_source_files) {
    vector<string> membership;
    membership.reserve(curr_membership_list.size());
    for (const auto &item : curr_membership_list) membership.push_back(item.first);
    int mission_id = 0;

    for (const auto &file : sdfs_source_files) {
        int temp = hash_string_to_int(file) % curr_membership_list.size();
        if (worker_mission_pair.find(membership[temp]) == worker_mission_pair.end()) {
            MapleMission new_mission = {mission_id++, PHASE_I, {file}};
            worker_mission_pair[membership[temp]] = new_mission;
        } else {
            worker_mission_pair[membership[temp]].files.push_back(file);
        }
    }
}

void server::maple_task_monitor(MapleMission &mission, string target_ip, string maple_exe, string sdfs_prefix) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    string response;
    try {
        /// Initialize socket connection.
        struct sockaddr_in serv_addr{};
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            throw runtime_error("Failure in create socket");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(this->mj_port);

        if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
            throw runtime_error("Invalid address");

        /// Try to connect to server, if fail then mark server as down.
        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            throw runtime_error("Connection failed");

        cout << "### begin send out maple request!" << endl;

        string maple_request = "maple_start " + maple_exe + " " + sdfs_prefix + " " + to_string(mission.mission_id);
        cout << maple_request << endl;
        for (auto &f : mission.files) {
            maple_request.push_back(' ');
            maple_request += f;
        }
        cout << maple_request << endl;
        const char *ls_file_c = maple_request.c_str();
        if (send(sock, ls_file_c, strlen(ls_file_c), 0) == -1)
            throw runtime_error("Sending maple mission failure");

        cout << "### send out maple request success!" << endl;

        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_I to PHASE_II");
        else {
            response = buffer;
            if (response == "maple_mission_receive") {
                mission.phase_id = PHASE_II;
                cout << target_ip << " entered phase II" << endl;
            }
        }

        buffer[0] = '\0';

        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_II to PHASE_III");
        else {
            response = buffer;
            if (response == "maple_mission_finished") {
                mission.phase_id = PHASE_III;
                cout << target_ip << " entered phase III" << endl;
            }
        }

        buffer[0] = '\0';

        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_III to PHASE_IV");
        else {
            response = buffer;
            if (response == "maple_mission_uploaded") {
                mission.phase_id = PHASE_IV;
                cout << target_ip << " entered phase IV" << endl;
            }
        }

        maple_juice_done_count_lock.lock();
        maple_juice_done_count++;
        maple_juice_done_count_lock.unlock();

        free_worker_lock.lock();
        free_worker.push(target_ip);
        free_worker_lock.unlock();

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        cout << "### Try to redistribute maple work..." << endl;

        /// Wait for files to be redistribute
        auto time_stamp = get_curr_timestamp_milliseconds();
        while (true)
            if (get_curr_timestamp_milliseconds() - time_stamp > 12000) break;

        /// Wait for the availability of a new slave.
        string new_worker;
        while (true) {
            free_worker_lock.lock();
            if (free_worker.empty()) {
                free_worker_lock.unlock();
                continue;
            }
            new_worker = free_worker.front();
            free_worker.pop();
            free_worker_lock.unlock();
            break;
        }

        /// Reset the mission and send it to the free new slave.
        mission.phase_id = PHASE_I;
        thread(&server::maple_task_monitor, this, ref(mission), new_worker, maple_exe, sdfs_prefix).detach();
        cout << "### Maple work redistributed." << endl;
    }

    close(sock);
}

void server::maple_task_processor(int sock, string process_command) {
    /// Decode the received maple command.
    stringstream ss(process_command);
    string maple_exe, command, sdfs_prefix, curr_file;
    vector<string> files;
    string response;
    int mission_id = 0;
    ss >> command >> maple_exe >> sdfs_prefix >> mission_id;
    while (ss >> curr_file && !curr_file.empty())
        files.push_back(curr_file);

    response = "maple_mission_receive";
    const char *res = response.c_str();
    send(sock, res, strlen(res), 0);
    cout << "### Receive maple message success!" << endl;


    /// Fetch the exe file and processing files.
    string target_get_ip = check_file_exist(maple_exe);
    get_query_sender(maple_exe, maple_exe, target_get_ip);
    for (auto &f : files) {
        target_get_ip = check_file_exist(f);
        get_query_sender(f, f, target_get_ip);
    }
    cout << "### All required files obtained!" << endl;

    /// Start running maple task.

    /// Create temp files for processing maple intermediate output.
    system("touch files/fetched/result");
    if (TEN_LINES_READ)
        system("touch files/fetched/maple_exe_input");
    string line;
    string sys_command = "chmod +x files/fetched/" + maple_exe;
    system(sys_command.c_str());
    string maple_sys_command =
            "files/fetched/" + maple_exe + " < files/fetched/maple_exe_input >> files/fetched/result";

    /// For all the files fetched, perform maple job.
    for (auto &file : files) {
        string file_to_read = "files/fetched/" + file;
        if (TEN_LINES_READ) {
            string input_temp_str;
            ifstream infile(file_to_read);
            int line_count = 0;
            while (getline(infile, line)) {
                line_count++;
                input_temp_str += (line + "\n");
                if (line_count == 10) {
                    line_count = 0;
                    ofstream ofs("files/fetched/maple_exe_input");
                    ofs << input_temp_str;
                    input_temp_str = "";
                    ofs.close();
                    system(maple_sys_command.c_str());
                }
            }
            if (line_count != 0) {
                ofstream ofs("files/fetched/maple_exe_input");
                ofs << input_temp_str;
                ofs.close();
                system(maple_sys_command.c_str());
            }
            cout << "Finish maple for " << file_to_read << endl;
            infile.close();
        } else {
            maple_sys_command = "files/fetched/" + maple_exe + " < " + file_to_read + " >> files/fetched/result";
            system(maple_sys_command.c_str());
        }

    }
    if (TEN_LINES_READ)
        system("rm files/fetched/maple_exe_input");
    cout << "### Finished maple tasks!" << endl;


    /// Extract maple result to local files
    cout << "Begin extracting files" << endl;
    ifstream infile("files/fetched/result");
    map<int, ofstream> of_map;
    while (getline(infile, line)) {
        stringstream line_ss(line);
        string key;
        line_ss >> key;
        int hashed_key = hash_string_to_int(key) % 9;
        if (of_map.find(hashed_key) == of_map.end()) {
            string out_file =
                    "files/fetched/" + sdfs_prefix + "_" + to_string(hashed_key) + "_" + to_string(mission_id);
            string temp_sys = "touch " + out_file;
            system(temp_sys.c_str());
            of_map[hashed_key].open(out_file);
        }
        of_map[hashed_key] << line << "\n";
    }
    for (auto &item : of_map) item.second.close();
    infile.close();
    cout << "Finish extracting files" << endl;
    system("rm files/fetched/result");


    response = "maple_mission_finished";
    res = response.c_str();
    send(sock, res, strlen(res), 0);


    /// Upload maple output files to sdfs
    cout << "Begin uploading files..." << endl;

    for (auto &item : of_map) {
        string out_file_name = sdfs_prefix + "_" + to_string(item.first) + "_" + to_string(mission_id);
        string out_file_fetched = curr_dir + "/files/fetched/" + out_file_name;
        cout << "### Uploading " << out_file_fetched << endl;
        maple_juice_put(out_file_fetched, out_file_name);
    }

    response = "maple_mission_uploaded";
    res = response.c_str();
    send(sock, res, strlen(res), 0);
    cout << "### Maple results uploaded!" << endl;
    close(sock);

    string sys_com = "rm files/fetched/" + sdfs_prefix + "_*";
    system(sys_com.c_str());
}


void server::juice_task_monitor(JuiceMission &mission, string target_ip, string juice_exe, string sdfs_dest,
                                int delete_input) {
    int sock = 0;
    char buffer[MAX_BUFFER_SIZE] = {0};
    string response;
    try {
        /// Initialize socket connection.
        struct sockaddr_in serv_addr{};
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            throw runtime_error("Failure in create socket");

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(this->mj_port);

        if (inet_pton(AF_INET, target_ip.c_str(), &serv_addr.sin_addr) <= 0)
            throw runtime_error("Invalid address");

        /// Try to connect to server, if fail then mark server as down.
        if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
            throw runtime_error("Connection failed");

        cout << "### begin send out juice request!" << endl;

        string juice_request =
                "juice_start " + juice_exe + " " + sdfs_dest + " " + to_string(mission.mission_id) + " " +
                to_string(delete_input);
        cout << juice_request << endl;
        for (auto &f : mission.prefixes) {
            juice_request.push_back(' ');
            juice_request += f;
        }


        /// Send out juice commad.
        cout << juice_request << endl;
        const char *ls_file_c = juice_request.c_str();
        if (send(sock, ls_file_c, strlen(ls_file_c), 0) == -1)
            throw runtime_error("Sending maple mission failure");

        cout << "### send out juice request success!" << endl;


        /// PHASE_I to PHASE_II: receive juice command ack from slave.
        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_I to PHASE_II");
        else {
            response = buffer;
            if (response == "juice_mission_receive") {
                mission.phase_id = PHASE_II;
                cout << target_ip << " entered phase II" << endl;
            }
        }
        buffer[0] = '\0';


        /// PHASE_II to PHASE_III: receive juice mission finish report from slave.
        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_II to PHASE_III");
        else {
            response = buffer;
            if (response == "juice_mission_finished") {
                mission.phase_id = PHASE_III;
                cout << target_ip << " entered phase III" << endl;
            }
        }
        buffer[0] = '\0';

        /// PHASE_III to PHASE_IV: receive juice results uploaded from slave.
        if (read(sock, buffer, MAX_BUFFER_SIZE) == 0)
            throw runtime_error("Wrong mission phase: PHASE_III to PHASE_IV");
        else {
            response = buffer;
            if (response == "juice_result_uploaded") {
                mission.phase_id = PHASE_IV;
                cout << target_ip << " entered phase IV" << endl;
            }
        }

        /// Add to job_done_count and psuh current slave to free worker.
        maple_juice_done_count_lock.lock();
        maple_juice_done_count++;
        maple_juice_done_count_lock.unlock();

        free_worker_lock.lock();
        free_worker.push(target_ip);
        free_worker_lock.unlock();

    } catch (runtime_error &e) {
        std::cerr << "error: " << e.what() << std::endl;
        cout << "### Try to redistribute juice work..." << endl;
        string new_worker;

        /// Wait for files to be redistribute
        auto time_stamp = get_curr_timestamp_milliseconds();
        while (true)
            if (get_curr_timestamp_milliseconds() - time_stamp > 12000) break;

        /// Wait for the availability of a new slave.
        while (true) {
            free_worker_lock.lock();
            if (free_worker.empty()) {
                free_worker_lock.unlock();
                continue;
            }
            new_worker = free_worker.front();
            free_worker.pop();
            free_worker_lock.unlock();
            break;
        }

        /// Reset the mission and send it to the free new slave.
        mission.phase_id = PHASE_I;
        thread(&server::juice_task_monitor, this, ref(mission), new_worker, juice_exe, sdfs_dest,
               delete_input).detach();
        cout << "### Juice work redistributed." << endl;
    }

    close(sock);
}

void server::juice_task_processor(int sock, string process_command) {
    /// Decode the received juice command.
    stringstream ss(process_command);
    string juice_exe, command, sdfs_dest, curr_prefix;
    vector<string> prefixes;
    string response;
    int mission_id = 0, delete_input = 0;
    ss >> command >> juice_exe >> sdfs_dest >> mission_id >> delete_input;
    while (ss >> curr_prefix && !curr_prefix.empty())
        prefixes.push_back(curr_prefix);

    response = "juice_mission_receive";
    const char *res = response.c_str();
    send(sock, res, strlen(res), 0);
    cout << "### Receive juice message success!" << endl;
    string resfile = sdfs_dest + "_" + to_string(mission_id);

    /// Fetch the exe file and processing files.
    string target_get_ip = check_file_exist(juice_exe);
    get_query_sender(juice_exe, juice_exe, target_get_ip);
    for (const auto &prefix : prefixes) {
        vector<string> files = check_all_exist_file_by_prefix(prefix);
        for (const auto &file : files) {
            target_get_ip = check_file_exist(file);
            get_query_sender(file, file, target_get_ip);
        }
    }
    cout << "### All required files obtained!" << endl;

    string sys_touch = "touch files/fetched/" + sdfs_dest + "_" + to_string(mission_id);

    /// Start running juice task.
    system(sys_touch.c_str());
    string line;
    string sys_command = "chmod +x files/fetched/" + juice_exe;
    system(sys_command.c_str());

    for (auto &prefix : prefixes) {
        sys_command = "cat files/fetched/" + prefix + "*|./files/fetched/" + juice_exe + " >> files/fetched/" + resfile;
        system(sys_command.c_str());
        cout << "Finish juice for " << prefix << endl;
    }
    cout << "### Finished juice tasks!" << endl;


    response = "juice_mission_finished";
    res = response.c_str();
    send(sock, res, strlen(res), 0);


    /// Upload juice output files to sdfs.
    maple_juice_put(curr_dir + "/files/fetched/" + resfile, resfile);

    if (delete_input == 1)
        for (auto prefix : prefixes)
            delete_all_file_by_prefix(prefix);


    response = "juice_result_uploaded";
    res = response.c_str();
    send(sock, res, strlen(res), 0);
    cout << "### juice results uploaded!" << endl;
    close(sock);
}