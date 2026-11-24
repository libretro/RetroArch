# The camera driver's preview against a scripted driver: who opens the
# camera, who polls it and who is handed its frames.

include ../../sample_objdir.mk

TARGET := camera_preview_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	camera_preview_test.c \
	$(CORE_DIR)/camera/camera_driver.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall $(INCDIRS) -DHAVE_V4L2

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g1 -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
