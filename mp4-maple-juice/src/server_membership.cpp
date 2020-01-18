/**
 * server_membership.cpp
 * Implementation of membership list funcs in server_func.h.
 */

#include "server_func.h"
#include "server_sdfs.h"

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
    stored_sdfs_files_lock.lock();
    cout << "### Files stored on this node are: " << endl;
    for (const auto &f : stored_sdfs_files) {
        cout << f.first << " " << f.second.time_stamp;
        if (f.second.is_master) cout << " I'm file master.";
        cout << endl;
    }
    stored_sdfs_files_lock.unlock();
    file_slaves_lock.lock();
    cout << "### My file slaves are: " << endl;
    for (const auto &f : file_slaves) {
        cout << f << endl;
    }
    file_slaves_lock.unlock();
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

void server::update_membership(vector<Member> received_list) {
    membership_list_lock.lock();
    bool to_send = false;
    auto m = received_list[0];
    if ((m.updated_time == ANNOUNCE && membership_list.find(m.ip_address) == membership_list.end())
        || m.updated_time == JOIN) {
        if (m.updated_time == ANNOUNCE) {
            write_log_file("Received 'announce' from " + string(m.ip_address), 0);
        }
        membership_list[m.ip_address] = m;
        membership_list[m.ip_address].updated_time = get_curr_timestamp_milliseconds();
        write_log_file(string(m.ip_address) + " added to my membership list", 1);
        if (m.updated_time == ANNOUNCE) {
            this_thread::sleep_for(chrono::nanoseconds(2000));
            handle_sdfs_join_rearrange();
        }
        m.updated_time = ANNOUNCE;
        to_send = true;
//        handle_sdfs_rearrange();
    } else if (m.updated_time == JOIN_SUCCESS) {
        for (auto item : received_list) {
            membership_list[item.ip_address] = item;
            membership_list[item.ip_address].updated_time = get_curr_timestamp_milliseconds();
            write_log_file(string(item.ip_address) + " added to my membership list", 1);
        }
        refresh_file_slaves();
    } else if (membership_list.find(m.ip_address) == membership_list.end() ||
               membership_list[m.ip_address].time_stamp >= m.time_stamp || !is_joined) {
        membership_list_lock.unlock();
        return;
    } else if (m.updated_time == FAILURE) {
        cout << "### Received 'failure' from " << m.ip_address << endl;
        write_log_file("Received 'failure' from " + string(m.ip_address), 0);
        membership_list.erase(m.ip_address);
        handle_sdfs_leave_rearrange();
        write_membership_list_to_log_file();
        to_send = true;
    } else if (m.updated_time == LEAVE) {
        cout << "### Received 'leave' from " << m.ip_address << endl;
        write_log_file("Received 'leave' from " + string(m.ip_address), 0);
        membership_list.erase(m.ip_address);
        handle_sdfs_leave_rearrange();
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
        if (it == membership_list.find(my_ip_address) || send_target.size() >= NUM_HEARTBEAT_SEND_TARGET) break;
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
                handle_sdfs_leave_rearrange();
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
