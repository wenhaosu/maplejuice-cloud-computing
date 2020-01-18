/**
 * grep.h
 * General functions and parameters for both server and client.
 */

#ifndef GENERAL_H
#define GENERAL_H

#include <iostream>
#include <ifaddrs.h>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/// The maximum size of single buffer
#define MAX_BUFFER_SIZE 4096

#define QUERY_PORT 8000
#define HB_PORT 8001
#define SDFS_PORT 8002


/**
 * Return the IP address of current machine.
 * Cited from: https://gist.github.com/quietcricket/2521037
 *
 * Returns:
 *      Return the ip address of current machine in string format.
 */
std::string get_my_ip_address();

#endif //GENERAL_H
