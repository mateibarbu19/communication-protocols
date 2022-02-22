#include "utils.h"
#include <cstddef>
#include <cstring>
#include <ctype.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

bool only_digits(const char *s, const size_t len) {
  bool ok = true;
  size_t i;
  while (ok && i < len) {
    ok = isdigit(s[i]);
    i++;
  }
  return ok;
}

void disable_nagle(const int sockfd) {
  static const int optval = 1;
  int ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
  DIE(ret < 0, "[SERVER] Disabling Nagle's algorithm failed");
}

enum operation interpret_command(const char *buffer) {
  if (strncmp(buffer, "subscribe", 9) == 0) {
    return SUBSCRIBE;
  }
  if (strncmp(buffer, "unsubscribe", 11) == 0) {
    return UNSUBSCRIBE;
  }
  if (strncmp(buffer, "exit", 4) == 0) {
    return EXIT;
  }
  return NONE;
}

int send_ok(const int sockfd) {
  static const char buffer[] = "ok";
  return send_msg(sockfd, buffer, sizeof(buffer) - 1);
}

void send_exit(const int sockfd) {
  static const char buffer[] = "exit";
  unsigned int nr;
  nr = htonl(sizeof(buffer) - 1);
  send(sockfd, &nr, sizeof(nr), MSG_NOSIGNAL);
  send(sockfd, buffer, sizeof(buffer) - 1, MSG_NOSIGNAL);
}

int send_msg(const int sockfd, const char *buffer, const size_t len) {
  unsigned int nr;
  ssize_t n;
  nr = htonl(len);
  n = send(sockfd, &nr, sizeof(nr), 0);
  DIE(n != sizeof(nr), "Could not send message length");
  n = send(sockfd, buffer, len, 0);
  DIE(n != (ssize_t) len, "Could not send message");
  return n;
}

int recv_msg(const int sockfd, char* buffer, const size_t buffer_len) {
  unsigned int nr = 0;
  ssize_t n = recv(sockfd, &nr, sizeof(nr), MSG_WAITALL);
  DIE (n != sizeof(nr), "Receiving a message length has failed!");
  size_t len = ntohl(nr);
  DIE(len > buffer_len, "Cannot receive a message with length greater than \
                         buffer size");
  memset(buffer, 0, buffer_len);
  n = recv(sockfd, buffer, len, MSG_WAITALL);
  DIE(n != (ssize_t) len, "Receiving a message data with specific length has failed!");
  return len;
}

void terminate_tcp_socket(const int sockfd) {
  send_exit(sockfd);
  int ret = shutdown(sockfd, SHUT_RDWR);
  DIE(ret < 0, "Shutdown on TCP socket failed");
  ret = close(sockfd);
  DIE(ret < 0, "Closing TCP socket failed");
}