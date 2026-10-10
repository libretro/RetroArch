# frontend/drivers/android_lifecycle.h: the lifecycle handshakes between
# the NativeActivity callbacks, the app thread and a window holder.

include ../../sample_objdir.mk

TARGET   := android_lifecycle_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := android_lifecycle_test.c \
           $(LRC)/rthreads/retro_eventcount.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/encodings/encoding_utf.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS -I$(LRC)/include
LDFLAGS += -lpthread
OBJS    := $(call sample_objs,$(SOURCES:.c=.o))

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
check: $(TARGET)
	./$(TARGET)
clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
