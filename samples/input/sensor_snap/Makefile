ROOT := ../../..
TARGET := sensor_snap_test
CC ?= cc
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -DHAVE_THREADS \
          -I$(ROOT) -I$(ROOT)/libretro-common/include
SOURCES := sensor_snap_test.c \
           $(ROOT)/libretro-common/rthreads/rthreads.c \
           $(ROOT)/libretro-common/features/features_cpu.c \
           $(ROOT)/libretro-common/time/rtime.c
LDFLAGS += -lpthread
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES) $(ROOT)/input/input_sensor_snap.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean
