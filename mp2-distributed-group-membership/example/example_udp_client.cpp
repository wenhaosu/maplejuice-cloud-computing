/*
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <chrono>
#include <iostream>
#include <vector>

#define BUFSIZE sizeof(member)

using namespace std;

uint64_t get_curr_timestamp() {
    return (uint64_t) chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

struct member {
    char ip_address[14];
    uint64_t time_stamp;
    uint64_t updated_time;
};

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;

    /* check command line arguments */
    if (argc != 3) {
        fprintf(stderr, "usage: %s <hostname> <port>\n", argv[0]);
        exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *) server->h_addr,
          (char *) &serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* get a message from the user */


    member a = {"172.22.154.5", htonl(get_curr_timestamp()), htonl(get_curr_timestamp())};

    member b = {"172.22.152.12", htonl(get_curr_timestamp()), htonl(get_curr_timestamp())};

    member c = {"172.22.155.5", htonl((uint64_t) 12), htonl((uint64_t) 123)};

    member member_list[3] = {a, b, c};


    char buf[sizeof(member_list)];
    bzero(buf, sizeof(member_list));
    printf("Sending item...");

    std::cout << a.ip_address << " " << ntohl(a.time_stamp) << " " << ntohl(a.updated_time) << std::endl;
    std::cout << a.ip_address << " " << ntohl(b.time_stamp) << " " << ntohl(b.updated_time) << std::endl;
    std::cout << a.ip_address << " " << ntohl(c.time_stamp) << " " << ntohl(c.updated_time) << std::endl;
    std::cout << "size: " << sizeof(member_list) << std::endl;

    memcpy(buf, &member_list, sizeof(member_list));

    /* send the message to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, sizeof(buf), 0, reinterpret_cast<const sockaddr *>(&serveraddr), serverlen);
    if (n < 0)
        error("ERROR in sendto");

//    /* print the server's reply */
//    n = recvfrom(sockfd, buf, strlen(buf), 0, reinterpret_cast<sockaddr *>(&serveraddr),
//                 reinterpret_cast<socklen_t *>(&serverlen));
//    if (n < 0)
//        error("ERROR in recvfrom");
//    printf("Echo from server: %s", buf);
    return 0;
}