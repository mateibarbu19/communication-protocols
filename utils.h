#pragma once

#include <netinet/in.h>
#include <string>
#include <sys/socket.h>

struct package_t {
  struct sockaddr_in udp_sender;
  char topic[50];
  char data_type;
  char content[1500];
} __attribute__((packed));
typedef struct package_t package;

struct package_wrapper_t {
  unsigned int pack_len;
  package pack;
} __attribute__((packed));
typedef struct package_wrapper_t package_wrapper;


enum operation { NONE, SUBSCRIBE, UNSUBSCRIBE, EXIT };

enum operation interpret_command(const char *buffer);

/* This functions check whether a string contains only digits. */
bool only_digits(const char *s, const size_t len);

void disable_nagle(const int sockfd);

int send_msg(const int sockfd, const char *buffer, const size_t len);

int recv_msg(const int sockfd, char* buffer, const size_t buffer_len);

int send_ok(const int sockfd);

void send_exit(const int sockfd);

void terminate_tcp_socket(const int sockfd);

/* Error checking macro */
#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

#define PAYLOAD_LEN                                                            \
  (sizeof(package) - sizeof(struct sockaddr_in))
#define BUF_LEN sizeof(package) // dimensiunea maxima a calupului de date
#define ID_LEN 10