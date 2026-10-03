# play_feature_delivery.c on the host, against the Android stubs: a core
# download asks Java with the status lock released.
#
#   make check
#   make check SANITIZER=address,undefined
#   make check SANITIZER=thread

TARGET    := play_feature_delivery_test
CORE_DIR  := ../../..
LRC       := $(CORE_DIR)/libretro-common
SANITIZER ?= $(SAN)

SOURCES_C := \
	play_feature_delivery_test.c \
	$(LRC)/rthreads/rthreads.c \
	$(LRC)/string/stdstring.c \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/lists/string_list.c \
	$(LRC)/encodings/encoding_utf.c \
	$(LRC)/compat/compat_strldup.c

CFLAGS  += -g -O1 -Wall -std=gnu99 -D_GNU_SOURCE -DANDROID -DHAVE_THREADS \
           -DRARCH_INTERNAL -I$(CORE_DIR)/tools/platform_stubs/android \
           -I$(CORE_DIR) -I$(LRC)/include
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES_C)
	$(CC) $(CFLAGS) -o $@ $(SOURCES_C) $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean
