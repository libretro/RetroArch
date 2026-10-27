include ../../sample_objdir.mk

TARGET := video_player_test

LIBRETRO_COMM_DIR := ../../..

# The player alone against a scripted stream (the test supplies the
# image_transfer stream calls). THREADS=0 builds the single-threaded
# shape, where every decode is the polling thread's.
SOURCES := \
	video_player_test.c \
	$(LIBRETRO_COMM_DIR)/formats/video_player.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifeq ($(FAULT_ALLOC),1)
   CFLAGS += -DVP_TEST_CALLOC_FAULTS
   LDFLAGS += -Wl,--wrap=calloc
endif

ifneq ($(THREADS),0)
   ifeq ($(FAULT_ALLOC),1)
      LDFLAGS += -Wl,--wrap=sthread_create
   endif
   CFLAGS  += -DHAVE_THREADS
   SOURCES += $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
              $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
              $(LIBRETRO_COMM_DIR)/rthreads/tpool.c
   ifneq ($(OS),Windows_NT)
      LDFLAGS += -lpthread
   endif
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
