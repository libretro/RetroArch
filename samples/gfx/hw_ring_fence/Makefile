TARGET := hw_ring_fence_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# The handoff under test is the tree's own header,
# gfx/video_thread_hw_fence.h; the only other dependency is rthreads.c
# for the second thread.
SOURCES := \
	hw_ring_fence_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

ifneq ($(SANITIZER),)
   SANFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif

all: $(TARGET) $(TARGET)_old

$(TARGET): $(SOURCES)
	$(CC) $(SANFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

# One fence per slot, signalled after every frame, as it was. Built
# without a sanitizer: what it must report is its own count.
$(TARGET)_old: $(SOURCES)
	$(CC) $(CFLAGS) -DOLD_SINGLE_FENCE -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)
	@if ./$(TARGET)_old > /dev/null 2>&1; then \
	   echo "FAIL: the single re-signalled fence passed; the reproducer has stopped reproducing"; \
	   exit 1; \
	else \
	   echo "[pass] the single re-signalled fence this replaced still fails"; \
	fi

clean:
	rm -f $(TARGET) $(TARGET)_old

.PHONY: all check clean
