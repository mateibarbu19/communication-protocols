# Homework 2 Communication Protocols

## Genernal description

The server will connect the clients from the platform with a `brocker`
role. The goal is to publish messages. A TCP client will connect to the
server, the server can receive input commands from the keyboard (only `exit`
command). The TCP client will display the messages from the server. UDP
clients only send the messages to the server, and then, the server sends
them to the subscribed clients. The server has a store-and-forward feature
to be used when the TCP client is disconnected.

## Basic Protocol

The premise is that at the application layer in the network stack, if we
use TCP, we are guaranteed that each byte is received reliably and in order.
However, this does not mean that bytes do not arrive at the same speed. Hence,
sometimes an entity can receive either an incomplete message or two message
in one.  This is when the `MSG_WAITALL` comes into play. `man recv` tells
the programmer that if `MSG_WAITALL` flag is used, a incomplete message
cannot be received without an error.

*My solution* is before sending a message to send the message length of size
`sizeof(unsigned int)`. Whenever, an entity wants to receive a message, it
first enters a blocked state waiting for `sizeof(unsigned int)` bytes. Only
after this, the message is read. Thus, an entity always reads a complete
message and nothing more.

### Implementation

This ABI is provided in the `utils.h` file. Both `send_msg` and `recv_msg`
are provided for the programme. Also, general use functions such as:

- `disable_nagle` - `terminate_tcp_socket` - send of acknowledgment and
communication termination messages.

## Server

### Implementation

The main logic is implemented in an infinite while loop  in which the server
performs an I/O multiplexing of:

- the standard input (`STDIN_FILENO`) - `udp_listen_sockfd` -
`tcp_listen_sockfd` - TCP clients socket file descriptors

*My solution* is to use three Hash tables (`unordered_map`, `O(1)` time
complexity):

- `topics`: All the messages related to one topic and all the subscribed
IDs are found in each entry.  - `clients`: All the information regarding
one client is stored in an entry. This information contains: connected
status `is_connected`, socket file descriptor `sockfd` and a Hash table
of subscribed topics.  - `socket_ids`: The client ID corresponding to one
socket is stored in each entry.

Thus, the server retains all the UDP messages it has ever received. Each
client stores in its `subscribed_topics` table which was the last message
it received. If the stored-and-forward flag was not set, this value will be
infinity. This results in a simple solution which sends every time a client
reconnects all the messages from the index of the last one received. If this
value is infinity, the loop is just never entered.

The server does not temper with any UDP datagrams received leaving the
interpretation of the content to the subscriber. This reduces unnecessary
computation.

Anytime a client contacts the server, the program checks for that ID to not
be already registered and processes subscriptions requests.

Note: Change of subscription type is not yet possible. If use of C++17 library
is possible, `insert_or_assign` function would come in handy at parsing the
subscriber requests.

### Error handling

- if any critical operation using sockets receives an error the program dies.

- any message from UDP clients will not contain `\0` in the data field,
nor in the topic.

## Subscriber

### Implementation

*My solution* tries to implement the client as a finite-state machine. The
`pending_operation` variable more or less denotes the state in which the
client is in.

States:

- `EXIT`: the client has to exit, entering this state either by a input
command or if the server has shutdown.  - `SUBSCRIBE`/`UNSUBSCRIBE`:
the client has read a input subscription related command and then, after
which it is waiting for the server response. It cannot receive any other
input commands until the server responds.  - `NONE`: the program can read
user input commands and send them to the server. Note any server received
messages in this state are *currently* discarded.

Note: These are not all the theoretical states in which the program might
be found, but rather the practical ones useful for this homework.

### Error handling

- if any critical operation using sockets receives an error, the program dies.

## Testing

```
sudo python3 test.py
```