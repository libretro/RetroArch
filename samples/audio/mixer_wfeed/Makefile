TARGET := mixer_wfeed_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps \
	-I../../../deps/stb

# The codec gates are what compile the windowed feeder in at all; with
# none of them the whole path under test is #ifdef'd out.
CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -DHAVE_THREADS \
	-DHAVE_AUDIOMIXER -DHAVE_MENU -DHAVE_CONFIGFILE -DHAVE_RVORBIS \
	-DHAVE_RWAV -DHAVE_RWEBM -D_GNU_SOURCE
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := mixer_wfeed_test.o stubs.o \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.o \
	$(LIBRETRO_COMM_DIR)/file/file_path.o \
	$(LIBRETRO_COMM_DIR)/string/stdstring.o \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.o \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.o \
	$(LIBRETRO_COMM_DIR)/lists/string_list.o \
	$(LIBRETRO_COMM_DIR)/time/rtime.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
