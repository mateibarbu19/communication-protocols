#pragma once

#include "utils.h"
#include <string>
#include <unordered_map>
#include <vector>

using std::string;
using std::unordered_map;
using std::vector;

struct topic_info_t {
  vector<package_wrapper> all_messages;
  vector<string> subscribed_ids;
};
typedef struct topic_info_t topic_info;

struct subscriber_t {
  bool is_connected;
  int sockfd;
  unordered_map<string, int> subscribed_topics;
};
typedef struct subscriber_t subscriber;

#define INF 2147483647
#define MAX_CLIENTS 100