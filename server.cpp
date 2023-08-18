#include <bits/stdc++.h>
#include "utils.h"

using namespace std;

// (fd, id) of connected clients
map<int, string> connected_clients;

// (topic, fd of subscribers to topic)
map<string, set<int>> subscribers;

// (topic, id of subscribers to topic with sf)
map<string, set<string>> sf;

// (fd, (ip, port)) for clients waiting for id validation
map<int, pair<in_addr, in_port_t>> waiting;

// (id, queue of messages)
map<string, queue<tcp_packet>> sf_queues;

void update_poll(int pos, int &nfds, vector<pollfd> &pfds) {

    for (int j = pos; j < nfds - 1; j++) {
        pfds[j] = pfds[j + 1];
    }
    
    pfds.pop_back();
    nfds--;
}

int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    int udp_sockfd, tcp_listen_sockfd;
    struct sockaddr_in servaddr;
    uint16_t port;
    message p;
    vector<pollfd> pfds;
    pollfd conn;
    int nfds = 0;
    if (argc != 2) {
        cout << "usage: " << argv[0] << " <port>\n";
        return 0;
    }

    int rc = sscanf(argv[1], "%hu", &port);
    if (rc != 1) {
        perror("Given port is invalid\n");
        exit(-1);
    }

    // creating the upd socket
    if ( (udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("udp socket creation failed\n");
        exit(-1);
    }

    // creating the tcp listen socket
    if ( (tcp_listen_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("tcp socket creation failed\n");
        exit(-1);
    }

    // disable nagle
    int enable = 1;
    if (setsockopt(tcp_listen_sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("Nodelay failed");

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    // binding the udp socket to the given port
    if ( bind(udp_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("udp bind failed\n");
        exit(-1);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    // binding the tcp socket to the given port
    if ( bind(tcp_listen_sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("tcp bind failed\n");
        exit(-1);
    }

    // listening on the listen socket
    rc = listen(tcp_listen_sockfd, 1000);
    if (rc < 0) {
        perror("listen");
        exit(-1);
    }
    
    pfds.push_back(conn);
    pfds[nfds].fd = udp_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds.push_back(conn);
    pfds[nfds].fd = tcp_listen_sockfd;
    pfds[nfds].events = POLLIN;
    nfds++;

    pfds.push_back(conn);
    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN; 
    nfds++;
    
    bool running = 1;

    while (running) {
        int rc = poll(&pfds[0], nfds, -1);
        if (rc < 0) {
            perror("poll");
            exit(-1);
        }

        for (int i = 0; i < nfds; i++) {
            if ((pfds[i].revents & POLLIN)) {
                if (pfds[i].fd == udp_sockfd) {
                    // we received a message from a udp client to be posted
                    int n;
                    struct sockaddr_in cliaddr;
                    memset(&p, 0, sizeof(p));
                    memset(&cliaddr, 0, sizeof(cliaddr));
                    socklen_t len = sizeof(cliaddr);

                    n = recvfrom(udp_sockfd, &p, sizeof(p), 0, (struct sockaddr *) &cliaddr, &len);
                    if (n < 0) {
                        perror("Receive failed\n");
                        exit(-1);
                    }
                    
                    tcp_packet sent_packet;
                    memset(&sent_packet, 0, sizeof(sent_packet));
                    sent_packet.type = 2;
                    memcpy(sent_packet.payload, &p, sizeof(p));
                    sent_packet.ip = cliaddr.sin_addr.s_addr;
                    sent_packet.port = cliaddr.sin_port;
                    // sending the message to all connected subscribers of the topic
                    for (int j = 3; j < nfds; j++) {
                        if (subscribers[p.topic].find(pfds[j].fd) != subscribers[p.topic].end()) {
                            send_all(pfds[j].fd, &sent_packet, sizeof(sent_packet));
                        }
                    }

                    // saving the message for disconnected subscribers with store and forward
                    for (auto it : sf[p.topic]) {
                        sf_queues[it].push(sent_packet);
                    }

                } else if (pfds[i].fd == tcp_listen_sockfd) {
                    // we have a new connection to the server
                    struct sockaddr_in cliaddr;
                    memset(&cliaddr, 0, sizeof(cliaddr));
                    socklen_t len = sizeof(cliaddr);
                    int newsockfd = accept(tcp_listen_sockfd, (struct sockaddr *)&cliaddr, &len);
                    if (newsockfd < 0) {
                        perror("accept");
                        exit(-1);
                    }
                    
                    int enable = 1;
                    if (setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
                        perror("Nodelay failed");
                    // the new accepted connection will not be valid until the ID is sent and verified
                    // until then it stays in the waiting queue
                    waiting[newsockfd].first = cliaddr.sin_addr;
                    waiting[newsockfd].second = cliaddr.sin_port;
                   
                    // adding the fd to the poll
                    pfds.push_back(conn);
                    pfds[nfds].fd = newsockfd;
                    pfds[nfds].events = POLLIN;
                    nfds++;

                } else if (pfds[i].fd == STDIN_FILENO) {
                    // we got an exit command and the server needs to stop
                    char command[100];
                    memset(&command, 0, sizeof(command));

                    scanf("%s", command);

                    if (strcmp(command, "exit") == 0) {
                        running = 0;
                        break;
                    } else {
                        cout << "Invalid command\n";
                        continue; 
                    }
                } else {
                    // receiving a packet from a tcp client
                    tcp_packet recv_packet;
                    int rc = recv_all(pfds[i].fd, &recv_packet, sizeof(recv_packet));
                    
                    if (rc < 0) {
                        perror("error");
                        exit(-1);
                    }

                    if (rc == 0) {
                        // connection closed
                        close(pfds[i].fd);

                        // updating the poll array
                        update_poll(i, nfds, pfds);

                        cout << "Client " << connected_clients[pfds[i].fd] << " disconnected.\n";
                        connected_clients.erase(pfds[i].fd);
                    } else {
                        switch (recv_packet.type) {
                            case 0:
                            {
                                // subscribe request
                                subscribe msg = *((subscribe *)recv_packet.payload);
                                subscribers[msg.topic].insert(pfds[i].fd);
                                if (msg.sf == 1) {
                                    sf[msg.topic].insert(connected_clients[pfds[i].fd]);
                                }
                                break;
                            }
                            case 1:
                            {
                                // unsubscribe request
                                subscribe msg = *((subscribe *)recv_packet.payload);
                                subscribers[msg.topic].erase(pfds[i].fd);
                                if (sf[msg.topic].find(connected_clients[pfds[i].fd]) != sf[msg.topic].end()) {
                                    sf[msg.topic].erase(connected_clients[pfds[i].fd]);
                                }
                                break;
                            }
                            case 3:
                            {
                                // connect request
                                char id[11];
                                strcpy(id, recv_packet.payload);
                                
                                // checking if a user with the same ID is already connected
                                bool ok = 1;
                                for (auto it : connected_clients)
                                    if (it.second.compare(id) == 0) {
                                        cout << "Client " << id << " already connected.\n";
                                        ok = 0;
                                        break;
                                    }
                                
                                if (ok) {
                                    // if not we have a new connection
                                    string str(id);
                                    connected_clients[pfds[i].fd] = str;

                                    cout << "New client " << id << " connected from " << inet_ntoa(waiting[pfds[i].fd].first) << ":" << ntohs(waiting[pfds[i].fd].second) << ".\n";
                                    
                                    // sending the stored messages
                                    while (!sf_queues[str].empty()) {
                                        tcp_packet sent_packet = sf_queues[str].front();
                                        sf_queues[str].pop();
                                        send_all(pfds[i].fd, &sent_packet, sizeof(sent_packet));
                                    }
                                } else {
                                    
                                    // we already have a connection with this ID so ask the user to disconnect
                                    tcp_packet sent_packet;
                                    memset(&sent_packet, 0, sizeof(sent_packet));
                                    sent_packet.type = 4;
                                    send_all(pfds[i].fd, &sent_packet, sizeof(sent_packet));
                                    close(pfds[i].fd);

                                    // updating the poll array
                                    update_poll(i, nfds, pfds);
                                }

                                waiting.erase(pfds[i].fd);
                                break;
                            }
                            case 4:
                            {
                                // disconnect request
                                close(pfds[i].fd);

                                update_poll(i, nfds, pfds);

                                cout << "Client " << connected_clients[pfds[i].fd] << " disconnected.\n";
                                connected_clients.erase(pfds[i].fd);
                            }

                        }
                    }
                }
            }
        }
    }

    // closing all the clients
    // sending disconnect request and closing both client sockets and
    // listen sockets 
    for (int i = 3; i < nfds; i++) {
        tcp_packet sent_packet;
        memset(&sent_packet, 0, sizeof(sent_packet));
        sent_packet.type = 4;
        send_all(pfds[i].fd, &sent_packet, sizeof(sent_packet));
        close(pfds[i].fd);
    }

    close(tcp_listen_sockfd);
    close(udp_sockfd);
    return 0;
}