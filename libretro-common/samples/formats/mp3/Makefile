include ../../sample_objdir.mk

TARGETS := rmp3_frame_test rmp3_gapless_test

LIBRETRO_COMM_DIR := ../../..

OBJS_rmp3_frame_test := $(call sample_objs,rmp3_frame_test.o \
	$(LIBRETRO_COMM_DIR)/formats/mp3/rmp3.o)
OBJS_rmp3_gapless_test := $(call sample_objs,rmp3_gapless_test.o \
	$(LIBRETRO_COMM_DIR)/formats/mp3/rmp3.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

rmp3_frame_test: $(OBJS_rmp3_frame_test)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

rmp3_gapless_test: $(OBJS_rmp3_gapless_test)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGETS) $(OBJS_rmp3_frame_test) $(OBJS_rmp3_gapless_test)

.PHONY: all clean
