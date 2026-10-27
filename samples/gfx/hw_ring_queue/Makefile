ROOT := ../../..
TARGET := hw_ring_queue_test
CFLAGS += -std=gnu99 -O1 -g1 -Wall -Wno-unused-function \
 -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_OPENGL \
 -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps
SRCS := hw_ring_queue_test.c \
 $(ROOT)/libretro-common/rthreads/rthreads.c \
 $(ROOT)/libretro-common/rthreads/retro_eventcount.c \
 $(ROOT)/libretro-common/compat/compat_strl.c \
 $(ROOT)/libretro-common/features/features_cpu.c \
 $(ROOT)/libretro-common/time/rtime.c
ifeq ($(OS),Windows_NT)
TARGET := $(TARGET).exe
CFLAGS += -flto -fwhole-program
LDFLAGS += -static -flto
else
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections -lpthread -lm
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SRCS) $(ROOT)/gfx/video_thread_hw.c $(ROOT)/gfx/video_thread_hw_fence.h
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean
