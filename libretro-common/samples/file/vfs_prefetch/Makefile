include ../../sample_objdir.mk

TARGET := vfs_prefetch_retarget_test
TARGET_VIEW := vfs_prefetch_view_test

LIBRETRO_COMM_DIR := ../../..

# vfs_prefetch.c, the read-ahead thread behind smb:// and nfs://
# streams, driven with a fetch callback in place of a network share.
COMMON := \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_prefetch.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

COMMON_OBJS := $(call sample_objs,$(COMMON:.c=.o))
OBJS := vfs_prefetch_retarget_test.o vfs_prefetch_view_test.o $(COMMON_OBJS)

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

all: $(TARGET) $(TARGET_VIEW)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): vfs_prefetch_retarget_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_VIEW): vfs_prefetch_view_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET) $(TARGET_VIEW)
	./$(TARGET)
	./$(TARGET_VIEW)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_VIEW) $(OBJS)

.PHONY: all check clean
