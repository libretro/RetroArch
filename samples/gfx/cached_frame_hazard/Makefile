include ../../sample_objdir.mk

TARGET := cached_frame_hazard_test
.DEFAULT_GOAL := all

LIBRETRO_COMM_DIR := ../../../libretro-common

# Lifetime stress plus a deterministic progress oracle over the shipping reader.
SOURCES := \
	cached_frame_hazard_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g1 -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

# TORTURE=1 widens the reader's generation-sample-to-arm window so the
# re-validate branch is actually exercised.  Run both: the plain build
# is the smoke test, the torture build is the regression test.
ifneq ($(TORTURE),)
   CFLAGS += -DTORTURE
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

PROGRESS_TARGET := cached_frame_progress_test$(suffix $(TARGET))
FALLBACK_TARGET := cached_frame_progress_fallback_test$(suffix $(TARGET))
READER := $(SAMPLE_OBJDIR)/reader.inc

$(READER): ../../../gfx/video_driver.c extract_reader.py
	@mkdir -p $(@D)
	python3 extract_reader.py $< $@

$(PROGRESS_TARGET): cached_frame_progress_test.c $(READER)
	$(CC) $(CFLAGS) -std=c89 -I$(SAMPLE_OBJDIR) -o $@ $< $(LDFLAGS)

$(FALLBACK_TARGET): cached_frame_progress_test.c $(READER)
	$(CC) $(CFLAGS) -std=c89 -DTEST_FALLBACK -I$(SAMPLE_OBJDIR) -o $@ $< $(LDFLAGS)

all: $(TARGET) $(PROGRESS_TARGET) $(FALLBACK_TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)
	./$(PROGRESS_TARGET)
	./$(FALLBACK_TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(PROGRESS_TARGET) $(FALLBACK_TARGET)

.PHONY: all check clean

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<
