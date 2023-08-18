#include <bits/stdc++.h>
#include "utils.h"

using namespace std;

void print_short(uint16_t x, uint8_t digits) {
    if (x == 0) {
        for (int i = 0; i < digits; i++)
            cout << 0;
        return;
    }
    
    print_short(x / 10, digits - 1);
    cout << x % 10;
}

void print_long(uint32_t x, uint8_t digits) {
    if (x == 0) {
        for (int i = 0; i < digits; i++)
            cout << 0;
        return;

    }
    
    print_long(x / 10, digits - 1);
    cout << x % 10;

}

uint8_t num_digits(uint32_t x) {
    uint8_t ans = 0;
    while (x > 0) {
        ans++;
        x /= 10;
    }

    return ans;
}

int main(int argc, char** argv) {

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int sockfd;
    struct sockaddr_in servaddr;
    char id[11];
    uint16_t port;
    struct pollfd pfds[2];
    int nfds = 0;
    char command[100], topic[100];

    if (argc != 4) {
        cout << "usage: " << argv[0] << " <id> " << " <server ip> " << " <port>\n";
        return 0;
    }
    strcpy(id, argv[1]);

    // saving the connection port
    int rc = sscanf(argv[3], "%hu", &port);
    if (rc != 1) {
        perror("Given port is invalid\n");
        exit(-1);
    }

    // creating the tcp socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(-1);
    }
    
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&servaddr, 0, socket_len);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &servaddr.sin_addr.s_addr);
    if (rc <= 0) {
        perror("inet_pton");
        exit(-1);
    }

    // disable nagle
    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int)) < 0)
        perror("Nodelay failed");

    // connecting to the server
    rc = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (rc < 0) {
        perror("connect");
        exit(-1);
    }

    // sending connection ID
    tcp_packet connect_packet;
    memset(&connect_packet, 0, sizeof(connect_packet));
    connect_packet.type = 3;
    memcpy(connect_packet.payload, id, strlen(id) + 1);
    send_all(sockfd, &connect_packet, sizeof(connect_packet));

    pfds[nfds].fd = STDIN_FILENO;
    pfds[nfds].events = POLLIN; 
    nfds++;

    pfds[nfds].fd = sockfd;
    pfds[nfds].events = POLLIN; 
    nfds++;

    while (1) {
        poll(pfds, nfds, -1);

        if ((pfds[0].revents & POLLIN) != 0) {
            // we have a command from the console            
            int sf = 0;
            memset(&command, 0, sizeof(command));
            memset(&topic, 0, sizeof(topic));

            scanf("%s", command);
            tcp_packet sent_packet;
            memset(&sent_packet, 0, sizeof(sent_packet));

            // sending a subscribe/unsubscribe packet to the server or exiting
            if (strcmp(command, "subscribe") == 0) {
                scanf("%s", topic);
                sent_packet.type = 0;
                subscribe *msg = (subscribe *) sent_packet.payload;
                memcpy(msg->topic, topic, strlen(topic) + 1);
                scanf("%d", &sf);
                msg->sf = sf;

                cout << "Subscribed to topic.\n";

            } else if (strcmp(command, "unsubscribe") == 0) {
                scanf("%s", topic);
                sent_packet.type = 1;
                subscribe *msg = (subscribe *) sent_packet.payload;
                memcpy(msg->topic, topic, strlen(topic) + 1);

                cout << "Unsubscribed from topic.\n";
            } else if (strcmp(command, "exit") == 0) {
                break;
            } else {
                cout << "Invalid command\n";
                continue;
            }

            // Use send_all function to send the packet to the server.
            send_all(sockfd, &sent_packet, sizeof(sent_packet));
        } else if ((pfds[1].revents & POLLIN) != 0) {
            tcp_packet recv_packet;
            int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
            if (rc <= 0) {
                break;
            }

            message p = *((message *) recv_packet.payload);

            if (recv_packet.type == 4) {
                    close(sockfd);
                    return 0;
            }
            
            char ip[20];
            inet_ntop(AF_INET, &(recv_packet.ip), ip, sizeof(ip));
            cout << ip << ":" << ntohs(recv_packet.port) << " " << p.topic << " - ";

            // converting the data types for printing
            switch(p.data_type) {
                case 0:
                {
                    cout << "INT" << " - ";
                    uint32_t *number = (uint32_t *) (p.content + 1);
                    if (p.content[0] == 1) {
                        cout << "-" << ntohl(*number) << '\n';
                    } else {
                        cout << ntohl(*number) << '\n';
                    }
                    break;
                }
                case 1:
                {
                    cout << "SHORT_REAL" << " - ";
                    uint16_t *number = (uint16_t *) (p.content);
                    uint16_t integer_part = ntohs(*number) / 100;
                    uint16_t fractional_part = ntohs(*number) % 100;
                    uint8_t digits;
                    
                    if (num_digits(ntohs(*number)) <= 2)
                        digits = 1;
                    else 
                        digits = num_digits(ntohs(*number)) - 2;
                    if (p.content[0] == 1)
                        cout << "-";
                    //cout << "digits are " << ntohs(*number) <<'\n'; 
                    print_short(integer_part, digits);
                    cout << '.';
                    print_short(fractional_part, 2);
                    cout<<'\n';

                    break;
                }
                case 2:
                {
                    cout << "FLOAT" << " - ";
                    uint32_t *number = (uint32_t *) (p.content + 1);
                    uint8_t *power = (uint8_t *) (p.content + 5);
                    //cout << (int)(*power) << '\n';
                    uint64_t pow_int = pow(10, (*power));

                    uint32_t integer_part = ntohl(*number) / pow_int;
                    uint32_t fractional_part = ntohl(*number) % pow_int;
                    uint8_t digits;
                    if (num_digits(ntohl(*number)) <= (*power))
                        digits = 1;
                    else 
                        digits = num_digits(ntohl(*number)) - (*power);

                    //cout << "digits are " << (int)digits <<'\n'; 
                    if (p.content[0] == 1)
                        cout << "-";
                    print_long(integer_part, digits);
                    if (*power > 0) {
                        cout << '.';
                        print_long(fractional_part, (*power));
                    }
                    cout<<'\n';

                    break;
                }
                case 3:
                {
                    cout << "STRING" << " - ";
                    for (int i = 0; i < 1500 && p.content[i] != '\0'; i++) {
                        cout<<p.content[i];
                    }
                    cout<<'\n';
                    break;
                }
            }
        } 
    }

    // sending a disconnect packet to the server and closing the connection
    tcp_packet sent_packet;
    memset(&sent_packet, 0, sizeof(sent_packet));
    sent_packet.type = 4;
    send_all(sockfd, &sent_packet, sizeof(sent_packet));
    close(sockfd);

    return 0;
}