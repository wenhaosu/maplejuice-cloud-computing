/**
 * server_sdfs.h
 * Define sdfs contents used in server.
 */

#ifndef SERVER_SDFS_H
#define SERVER_SDFS_H

#include <iostream>
#include <functional>
#include <string>
#include <vector>

#define SDFS_PATH "/files/sdfs"
#define FETCHED_PATH "/files/fetched"

#define NUM_VMS 10
#define REPLICA_NUM 4
#define QUORUM_W 4
#define QUORUM_R 1

struct FileInfo {
    bool is_master;
    uint64_t time_stamp;
    uint64_t file_size;
};

#endif //SERVER_SDFS_H
