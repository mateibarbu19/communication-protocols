# Matei Barbu <matei.barbu1905@stud.acs.upb.ro>

CC = g++
CFLAGS = -Wall -Wextra -std=c++11
DEBUG = -ggdb -O0
TARGETS = server subscriber

.PHONY: build clean

build: $(TARGETS)

%.o: %.cpp
	$(CC) $(CFLAGS) $(DEBUG) $^ -c -o $@

server: server.o utils.o
	$(CC) $^ -lstdc++ -o $@

subscriber: subscriber.o utils.o
	$(CC) $^ -lstdc++ -o $@

clean:
	rm -rf $(TARGETS) *.o