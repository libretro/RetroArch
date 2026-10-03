# ui/companion/companion_thumbs.c against fake still and animation
# decoders: cache, workers, cancel, animation switching, preview audio
# and the still-to-animation session handoff.

TARGET   := companion_thumbs_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := companion_thumbs_test.c \
           $(CORE_DIR)/ui/companion/companion_thumbs.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/rthreads/retro_eventcount.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/compat_strldup.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/encodings/encoding_utf.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS \
           -I$(CORE_DIR) -I$(LRC)/include
LDFLAGS += -lpthread
OBJS    := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean
