#ifndef UTILS
#define UTILS

#include <bits/stdc++.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/poll.h>
#include <netinet/tcp.h>

// udp packet structure to receive messages from udp clients
struct message {
    char topic[50];
    uint8_t data_type;
    char content[1500];
};

// structure for tcp packets
struct tcp_packet {
    int type; // 0 - subscribe, 1 - unsubscribe, 2 - message, 3 - connect, 4 - disconnect
    char payload[1600];
    in_addr_t ip;
    uint16_t port;
};

// subscribe request
struct subscribe {
    char topic[51];
    int sf;
};

int recv_all(int sockfd, void *buffer, size_t len);


int send_all(int sockfd, void *buffer, size_t len);
#endif