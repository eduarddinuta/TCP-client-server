#include "utils.h"

int recv_all(int sockfd, void *buffer, size_t len) {

  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;

    while(bytes_remaining) {
        bytes_received = recv(sockfd, buff + len - bytes_remaining, bytes_remaining, 0);
        bytes_remaining -= bytes_received;
    }
    
    return len;
}


int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = (char *)buffer;
    while(bytes_remaining) {
        bytes_sent = send(sockfd, buff + len - bytes_remaining, bytes_remaining, 0);
        bytes_remaining -= bytes_sent;
    }

  return len;
}

