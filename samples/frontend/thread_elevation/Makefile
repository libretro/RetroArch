# The thread elevation chain runner, driven with a scripted backend
# list: the order backends are tried in, the thread id a brokered one
# is handed, and a pending request refused on its own thread carrying
# on with the rest of the chain.
#
#   make check                          build and run
#   make check SANITIZER=address,undefined
#   make check SANITIZER=thread

TARGET    := thread_elevation_test
CORE_DIR  := ../../..
SANITIZER ?= $(SAN)

SOURCES_C := \
	thread_elevation_test.c \
	$(CORE_DIR)/frontend/thread_elevation.c \
	$(CORE_DIR)/frontend/thread_elevation/eevdf.c \
	$(CORE_DIR)/libretro-common/rthreads/rthreads.c

CFLAGS  += -g -O1 -Wall -Wextra -std=c89 -pedantic -Wno-long-long \
           -D_GNU_SOURCE -DHAVE_THREADS -DTHREAD_ELEVATION_BACKENDS_EXTERNAL \
           -I$(CORE_DIR) -I$(CORE_DIR)/libretro-common/include
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES_C)
	$(CC) $(CFLAGS) -o $@ $(SOURCES_C) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
