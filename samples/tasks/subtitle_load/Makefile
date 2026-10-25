include ../../sample_objdir.mk

TARGET := subtitle_load_test
ROOT := ../../..
LRC := $(ROOT)/libretro-common
THREADS ?= 1
SOURCES := subtitle_load_test.c $(ROOT)/tasks/task_subtitle.c \
           $(ROOT)/tasks/task_nbio_slice.c \
           $(LRC)/formats/subtitle/rsubtitle_srt.c $(LRC)/formats/subtitle/rsubtitle_vtt.c \
           $(LRC)/formats/subtitle/rsubtitle_vtt_css.c $(LRC)/formats/subtitle/rsubtitle_vtt_css_tree.c \
           $(LRC)/formats/subtitle/rsubtitle_vtt_selector.c \
           $(LRC)/formats/mp4/rmp4.c $(LRC)/formats/webm/rwebm.c $(LRC)/formats/data_transfer.c \
           $(LRC)/memmap/memmap.c \
           $(LRC)/queues/task_queue.c $(LRC)/rthreads/rthreads.c \
           $(LRC)/rthreads/retro_eventcount.c \
           $(LRC)/features/features_cpu.c $(LRC)/time/rtime.c \
           $(LRC)/streams/file_stream.c $(LRC)/vfs/vfs_implementation.c \
           $(LRC)/file/retro_dirent.c $(LRC)/file/file_path.c $(LRC)/file/file_path_io.c \
           $(LRC)/string/stdstring.c $(LRC)/string/rstrtod.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/compat/compat_strl.c $(LRC)/compat/compat_strldup.c \
           $(LRC)/compat/fopen_utf8.c
OBJS := $(call sample_objs,$(SOURCES:.c=.o))
LDFLAGS += -Wl,--wrap=filestream_read
CFLAGS += -DHAVE_RMP4 -DHAVE_RWEBM -O1 -g1 -std=gnu89 -Wall -Wextra -Wno-unused-parameter \
          -Werror=declaration-after-statement -I$(ROOT) -I$(LRC)/include
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
endif
ifneq ($(OS),Windows_NT)
CFLAGS += -DHAVE_MMAP -D_GNU_SOURCE
endif
ifeq ($(OS),Windows_NT)
LDLIBS += -lwinmm
else
LDLIBS += -lpthread -lm
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: all
	./$(TARGET)
	python3 embedded_check.py --malformed ./$(TARGET)
	python3 webm_check.py --malformed ./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS) subtitle-load-*.srt

.PHONY: all check clean
