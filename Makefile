CC      = gcc
CFLAGS  = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# ---- Targets --------------------------------------------------------

.PHONY: all test rest_api clean

all: test rest_api

# Run automated tests (no external dependencies)
test: test_linked_list
	./test_linked_list

test_linked_list: test_linked_list.c linked_list.c linked_list.h
	$(CC) $(CFLAGS) -o $@ test_linked_list.c linked_list.c $(LDFLAGS)

# REST API server (requires libmicrohttpd and libcjson)
rest_api: rest_api.c linked_list.c linked_list.h
	$(CC) $(CFLAGS) -o $@ rest_api.c linked_list.c \
		-lmicrohttpd -lcjson $(LDFLAGS)

clean:
	rm -f test_linked_list rest_api

# ---- Dependency -----------------------------------------------------
# sudo apt-get install libmicrohttpd-dev libcjson-dev
