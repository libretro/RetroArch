include ../../sample_objdir.mk

TARGET := vfs_prefetch_retarget_test

LIBRETRO_COMM_DIR := ../../..

# vfs_prefetch.c, the read-ahead thread behind smb:// and nfs://
# streams, driven with a fetch callback in place of a network share.
SOURCES := \
	vfs_prefetch_retarget_test.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_prefetch.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -W -std=gnu99 -g1 -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
ifneq (,$(findstring mingw,$(CC))$(findstring MINGW,$(MSYSTEM))$(findstring Windows,$(OS)))
   LDFLAGS += -lwinmm
else
   LDFLAGS += -lpthread -lrt
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean
