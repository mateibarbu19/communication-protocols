#include "server.h"
#include "utils.h"
#include <algorithm>
#include <arpa/inet.h>
#include <bits/stdint-uintn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <set>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

void usage_error(char *file) {
  fprintf(stderr, "Usage: %s server_port\n", file);
  exit(0);
}

int init_tcp_udp_listen_sockfd(int *tcp_listen_sockfd, int *udp_listen_sockfd,
                               int port_nr, fd_set *read_fds) {
  struct sockaddr_in serv_addr;
  int ret;

  *tcp_listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(*tcp_listen_sockfd < 0, "[SERVER] TCP socket open failed");
  disable_nagle(*tcp_listen_sockfd);
  static const int optval = 1;
  ret = setsockopt(*tcp_listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
  DIE(ret == -1, "setsockopt() reuse");

  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_nr);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  ret = bind(*tcp_listen_sockfd, (struct sockaddr *)&serv_addr,
             sizeof(struct sockaddr));
  DIE(ret < 0, "bind failed");

  ret = listen(*tcp_listen_sockfd, MAX_CLIENTS);
  DIE(ret < 0, "[SERVER] TCP listen failed");

  // add the new file descriptor (the listener) to read_fds set
  FD_SET(*tcp_listen_sockfd, read_fds);

  *udp_listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(*udp_listen_sockfd < 0, "[SERVER] UDP socket open failed");

  memset((char *)&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port_nr);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  ret = bind(*udp_listen_sockfd, (struct sockaddr *)&serv_addr,
             sizeof(struct sockaddr));
  DIE(ret < 0, "bind");

  // add the new file descriptor (the listener) to read_fds set
  FD_SET(*udp_listen_sockfd, read_fds);

  // the largest value currently in read_fds set
  return std::max(*tcp_listen_sockfd, *udp_listen_sockfd);
}

void update_subscriber_msg(subscriber &s, string &topic, int pos) {
  if (s.subscribed_topics[topic] != INF) {
    s.subscribed_topics[topic] = pos;
  }
}

void finish(int tcp_listen_sockfd, int udp_listen_sockfd) {
  int ret = shutdown(tcp_listen_sockfd, SHUT_RDWR);
  DIE(ret < 0, "[SERVER] Shutdown on TCP listen socket failed");
  ret = close(tcp_listen_sockfd);
  DIE(ret < 0, "[SERVER] Closing TCP listen socket failed");
  ret = close(udp_listen_sockfd);
  DIE(ret < 0, "[SERVER] Closing UDP listen socket failed");
}

int main(int argc, char **argv) {
  int tcp_listen_sockfd, udp_listen_sockfd, newsockfd, port_nr;
  struct sockaddr_in cli_addr;
  char buffer[BUF_LEN];
  string id, topic;
  int n, i, ret;
  socklen_t cli_len;
  package pack;

  fd_set read_fds;
  int max_fd;
  fd_set tmp_fds;

  // A HashTable with a corresponding list of all messages sent on that topic
  // and a list of all the currently subscribed clients.
  unordered_map<string, topic_info> topics;
  // for each id, return subscriber info
  unordered_map<string, subscriber> clients;
  // for each existing socket, return the corresponding id
  unordered_map<int, string> socket_ids;

  if (argc != 2) { // || !only_digits(argv[1], strlen(argv[1]))) {
    usage_error(argv[0]);
  }

  /* Preamble */
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  setvbuf(stderr, NULL, _IONBF, BUFSIZ);
  FD_ZERO(&read_fds);
  FD_ZERO(&tmp_fds);

  port_nr = atoi(argv[1]);
  DIE(port_nr < 0, "[SERVER] atoi on port char* failed");

  max_fd = init_tcp_udp_listen_sockfd(&tcp_listen_sockfd, &udp_listen_sockfd,
                                      port_nr, &read_fds);

  FD_SET(STDIN_FILENO, &read_fds);

  while (1) {
    tmp_fds = read_fds;

    ret = select(max_fd + 1, &tmp_fds, NULL, NULL, NULL);
    DIE(ret < 0, "[SERVER] select failed");

    for (i = 0; i <= max_fd; i++) {
      if (FD_ISSET(i, &tmp_fds)) {
        if (i == 0) {
          read(STDIN_FILENO, buffer, sizeof(buffer));

          if (interpret_command(buffer) == EXIT) {
            FD_CLR(tcp_listen_sockfd, &read_fds);
            FD_CLR(udp_listen_sockfd, &read_fds);
            FD_CLR(STDIN_FILENO, &read_fds);
            for (int j = 3; j <= max_fd; j++) {
              if (FD_ISSET(j, &read_fds)) {
                terminate_tcp_socket(newsockfd);
              }
            }
            finish(tcp_listen_sockfd, udp_listen_sockfd);
            return 0;
          }
        } else if (i == udp_listen_sockfd) {
          // a new UDP message has arrived
          socklen_t sock_len = sizeof(struct sockaddr_in);
          memset(&pack, 0, sizeof(pack));
          int len = recvfrom(i, pack.topic, PAYLOAD_LEN, 0,
                             (struct sockaddr *)&cli_addr, &sock_len);
          DIE(len < 0, "[SERVER] Receiving a UDP package failed");

          topic = std::string(pack.topic);
          if (topics.count(topic) == 0) {
            topics[topic] = {vector<package_wrapper>(), vector<string>()};
          }

          vector<string> subscribed_ids = topics[topic].subscribed_ids;
          len += sizeof(cli_addr);
          pack.udp_sender = cli_addr;
          topics[topic].all_messages.push_back({(unsigned int) len, pack});
          for (string &id_s : subscribed_ids) {
            if (clients[id_s].is_connected) {
              send_msg(clients[id_s].sockfd, (char*) &pack, len);
              update_subscriber_msg(clients[id_s], topic, topics[topic].all_messages.size());
            }
          }
        } else if (i == tcp_listen_sockfd) {
          // a new TCP client wants to connect
          cli_len = sizeof(cli_addr);
          newsockfd =
              accept(tcp_listen_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
          DIE(newsockfd < 0, "[SERVER] accept on a new TCP connection failed");
          // we are waiting for the client ID
          n = recv_msg(newsockfd, buffer, ID_LEN);

          buffer[ID_LEN] = '\0';
          id = std::string(buffer, strlen(buffer));
          if (clients.count(id) > 0 && clients[id].is_connected) {
            printf("Client %s already connected.\n", id.c_str());
            terminate_tcp_socket(newsockfd);
            continue;
          }

          disable_nagle(newsockfd);
          send_ok(newsockfd);
          printf("New client %s connected from %s:%d.\n", buffer,
                 inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
          if (clients.count(id) == 0) {
            clients[id] = {true, newsockfd, std::unordered_map<string, int>()};
          }
          // we add the new socket to read_fds set
          FD_SET(newsockfd, &read_fds);
          if (newsockfd > max_fd) {
            max_fd = newsockfd;
          }
          socket_ids[newsockfd] = id;
          clients[id].is_connected = true;
          clients[id].sockfd = newsockfd;

          // we send any stored items to that client
          for (pair<string, int> sub : clients[id].subscribed_topics) {
            for (size_t j = sub.second;
                 j < topics[sub.first].all_messages.size(); j++) {
              char *p = (char *) &(topics[sub.first].all_messages[j].pack);
              send_msg(newsockfd, p, topics[sub.first].all_messages[j].pack_len);
            }
            update_subscriber_msg(clients[id], sub.first, topics[sub.first].all_messages.size());
          }
        } else {
          // a TCP client has sent information
          n = recv_msg(i, buffer, sizeof(buffer));

          if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
            // connexion has closed
            printf("Client %s disconnected.\n", socket_ids[i].c_str());
            ret = close(i);
            DIE(ret < 0, "[SERVER] Cannot close a TCP client socket");
            // remove this socket from read_fds set
            FD_CLR(i, &read_fds);
            clients[socket_ids[i]].is_connected = false;
            clients[socket_ids[i]].sockfd = -1;
            socket_ids.erase(i);
          } else {
            enum operation op = interpret_command(buffer);
            if (op == SUBSCRIBE) {
              char *p = strchr(buffer, ' ');
              char *q = strrchr(buffer, ' ');
              if (p == NULL || q == NULL || q <= p) {
                continue;
              }
              topic = std::string(p + 1, q);
              id = socket_ids[i];
              if (clients[id].subscribed_topics.count(topic) == 0) {
                if (atoi(q + 1) == 0) {
                  clients[id].subscribed_topics[topic] = INF;
                } else {
                  clients[id].subscribed_topics[topic] =
                      topics[topic].all_messages.size();
                }
                topics[topic].subscribed_ids.push_back(id);
                send_ok(i);
              }
            } else if (op == UNSUBSCRIBE) {
              char *p = strchr(buffer, ' ');
              char *q = strchr(buffer, '\n');
              if (p == NULL || q <= p) {
                continue;
              }
              topic = std::string(p + 1, q);
              vector<string> &v = topics[topic].subscribed_ids;
              std::remove(v.begin(), v.end(), socket_ids[i]);
              v.erase(std::find(v.begin(), v.end(), socket_ids[i]));
              clients[id].subscribed_topics.erase(topic);
              send_ok(i);
            }
          }
        }
      }
    }
  }
}