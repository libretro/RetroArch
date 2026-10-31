include ../../sample_objdir.mk

EXE ?=
TARGET := translation_reply_test$(EXE)

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common
TRANSLATION_SOURCE ?= $(REPO_ROOT)/tasks/task_translation.c
RPNG ?= 1

# tasks/task_translation.c is #included by the test, which stands in
# for the frontend and for rpng; the scaler that draws the reply into
# the frame is the real one.
SOURCES := translation_reply_test.c \
           stubs.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL \
           -DHAVE_TRANSLATE -DHAVE_NETWORKING -DHAVE_ACCESSIBILITY -DHAVE_AUDIOMIXER \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps

ifeq ($(RPNG),1)
   CFLAGS += -DHAVE_RPNG
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDFLAGS += -lm

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

translation_reply_test.o: translation_reply_test.c $(TRANSLATION_SOURCE)
	$(CC) $(CFLAGS) -DTRANSLATION_SOURCE='"$(abspath $(TRANSLATION_SOURCE))"' -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) translation_reply_test translation_reply_test.exe

.PHONY: all check clean
