/**
 * grep.cpp
 * General functions and parameters for both server and client.
 */

#include "general.h"

std::string get_my_ip_address() {
    std::string ipAddress = "Unable to get IP Address";
    struct ifaddrs *interfaces = nullptr;
    struct ifaddrs *temp_addr = nullptr;
    int success = 0;
    /// Retrieve the current interfaces - returns 0 on success.
    success = getifaddrs(&interfaces);
    if (success == 0) {
        /// Loop through linked list of interfaces.
        temp_addr = interfaces;
        while (temp_addr != nullptr) {
            if (temp_addr->ifa_addr->sa_family == AF_INET)
                /// Check if interface is en0 which is the wifi connection on the iPhone.
                if (strcmp(temp_addr->ifa_name, "en0") != 0)
                    ipAddress = inet_ntoa(((struct sockaddr_in *) temp_addr->ifa_addr)->sin_addr);
            temp_addr = temp_addr->ifa_next;
        }
    }
    /// Free memory.
    freeifaddrs(interfaces);
    return ipAddress;
}