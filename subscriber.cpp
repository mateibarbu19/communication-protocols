#include "utils.h"
#include <arpa/inet.h>
#include <exception>
#include <iostream>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void usage(char *file) {
  fprintf(stderr, "Usage: %s client_id server_address server_port\n", file);
  exit(0);
}

void register_id(int server_sockfd, char *id) {
  char buffer[ID_LEN] = {0};
  send_msg(server_sockfd, id, std::min(strlen(id), (size_t) ID_LEN));
  recv_msg(server_sockfd, buffer, sizeof(buffer));

  if (strncmp(buffer, "ok", strlen("ok")) != 0) {
    fprintf(stderr, "[CLIENT] Server did not accept my ID");
    terminate_tcp_socket(server_sockfd);
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  int server_sockfd, n, ret, nr;
  struct sockaddr_in serv_addr;
  char buffer[BUF_LEN];
  enum operation pending_operation;
  double nr_double;
  package *msg;

  fd_set read_fds; // multimea de citire folosita in select()
  fd_set tmp_fds;  // multime folosita temporar

  if (argc != 4) {
    usage(argv[0]);
  }

  /* Preamble */
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  setvbuf(stderr, NULL, _IONBF, BUFSIZ);
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(server_sockfd < 0, "[CLIENT] socket open failed");
  disable_nagle(server_sockfd);

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  ret = inet_aton(argv[2], &serv_addr.sin_addr);
  DIE(ret == 0, "[CLIENT] inet_aton failed");

  ret =
      connect(server_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(ret < 0, "[CLIENT] connecting to server failed");

  register_id(server_sockfd, argv[1]);

  FD_SET(server_sockfd, &read_fds);
  FD_SET(STDIN_FILENO, &read_fds);

  pending_operation = NONE;
  while (pending_operation != EXIT) {
    tmp_fds = read_fds;

    ret = select(server_sockfd + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(ret < 0, "[CLIENT] select failed");

    if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
      // se citeste de la tastatura
      memset(buffer, 0, sizeof(buffer));
      n = read(STDIN_FILENO, buffer, sizeof(buffer));

      if (pending_operation != NONE) {
        fprintf(stderr, "Ignoring current command, waitting for server response on the last command...\n");
        continue;
      }

      pending_operation = interpret_command(buffer);

      if (pending_operation == SUBSCRIBE || pending_operation == UNSUBSCRIBE) {
        // we send the command to the server
        send_msg(server_sockfd, buffer, n);
      }
    } else if (FD_ISSET(server_sockfd, &tmp_fds)) {
      // received data from server
      n = recv_msg(server_sockfd, buffer, sizeof(buffer));

      if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
        // the server was closed
        ret = close(server_sockfd);
        DIE(ret < 0, "[CLIENT] Cannot close server TCP socket");
        exit(0);
      }

      ret = strncmp(buffer, "ok", 2);
      if (pending_operation == SUBSCRIBE && ret == 0) {
        printf("Subscribed to topic.\n");
        pending_operation = NONE;
      } else if (pending_operation == UNSUBSCRIBE && ret == 0) {
        printf("Unsubscribed from topic.\n");
        pending_operation = NONE;
      } else if (pending_operation != EXIT && buffer[0] == '@') {
        printf("%sAICI", buffer + 1);
        pending_operation = NONE;
      } else if (pending_operation != EXIT) {
        msg = (package *)buffer;
        printf("%s:%d - %s - ", inet_ntoa(msg->udp_sender.sin_addr), ntohs(msg->udp_sender.sin_port), msg->topic);
        switch (msg->data_type) {
        case 0:
          nr = ntohl(*(int *)(msg->content + 1));
          if (msg->content[0]) {
            nr = -nr;
          }
          printf("INT - %d\n", nr);
          break;
        case 1:
          printf("SHORT_REAL - %.2f\n", ntohs(*(uint16_t *)msg->content) / 100.0);
          break;
        case 2:
          nr_double = ntohl(*(uint32_t*)(msg->content + 1));
          nr = msg->content[5];
          while (nr--) {
            nr_double /= 10;
          }
          if (msg->content[0]) {
            nr_double = -nr_double;
          }
          printf("FLOAT - %.*lf\n", msg->content[5], nr_double);
          break;
        case 3:
          printf("STRING - %s\n", msg->content);
          break;
        default:
          break;
        }
        pending_operation = NONE;
      }
    }
  }

  terminate_tcp_socket(server_sockfd);

  return 0;
}
