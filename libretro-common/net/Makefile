TARGETS  = http_test net_ifinfo

INCFLAGS = -I../include -I../include/net

SOURCES := $(wildcard *.c) \
				../compat/compat.c
OBJS := $(SOURCES:.c=.o)

ifeq ($(DEBUG),1)
CFLAGS += -O0 -g
else
CFLAGS += -O2
endif
CFLAGS += -Wall -pedantic -std=gnu99

HTTP_TEST_C = \
				  net_http.c \
				  net_http_test.c \
				  net_compat.c \
				  ../compat/compat_strl.c

HTTP_TEST_OBJS := $(HTTP_TEST_C:.c=.o)

NET_IFINFO_C = \
					net_ifinfo.c \
					net_ifinfo_test.c

NET_IFINFO_OBJS := $(NET_IFINFO_C:.c=.o)

.PHONY: all clean

all: $(TARGETS)

%.o: %.c
	$(CC) $(INCFLAGS) $< -c $(CFLAGS) -o $@

http_test: $(HTTP_TEST_OBJS)
	$(CC) $(INCFLAGS) $(HTTP_TEST_OBJS) $(CFLAGS) -o $@

net_ifinfo: $(NET_IFINFO_OBJS)
	$(CC) $(INCFLAGS) $(NET_IFINFO_OBJS) $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS) $(HTTP_TEST_OBJS) $(NET_IFINFO_OBJS)
