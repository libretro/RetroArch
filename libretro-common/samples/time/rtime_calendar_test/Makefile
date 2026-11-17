include ../../sample_objdir.mk

CORE_DIR := ../../..
EXE ?=
TARGET      := rtime_calendar_test$(EXE)
# The same sources as Android builds them, against the stand-in NDK
# headers; the test serves the zone property. Linux only.
TARGET_TEST := rtime_calendar_android_test
SOURCES_C := rtime_calendar_test.c \
	$(CORE_DIR)/time/rtime.c \
	$(CORE_DIR)/compat/compat_strl.c
OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))
CFLAGS  += -Wall -I$(CORE_DIR)/include -DHAVE_THREADS -D_GNU_SOURCE -O1 -g1

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(TARGET_TEST)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
$(TARGET_TEST): $(SOURCES_C)
	$(CC) $(CFLAGS) -DANDROID -I$(CORE_DIR)/../tools/platform_stubs/android \
		$(SOURCES_C) -o $@ $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) rtime_calendar_test rtime_calendar_test.exe $(TARGET_TEST)
.PHONY: all clean
