/**
 * server_membership.h
 * Define membership list contents used in server.
 */

#ifndef SERVER_MEMBERSHIP_H
#define SERVER_MEMBERSHIP_H

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

#endif //SERVER_MEMBERSHIP_H
