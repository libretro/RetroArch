include ../../sample_objdir.mk

# The BTstack HID driver's thread, built on the host as C89 against a
# stand-in for libBTstack the test answers: once as on Apple, with a
# stand-in for CoreFoundation's run loop (cf_sdk/) the test runs, and
# once without one.
RARCH_DIR := ../../..
LRC       := $(RARCH_DIR)/libretro-common

LIB_SOURCES := \
	$(RARCH_DIR)/input/common/input_hid_common.c \
	$(wildcard $(RARCH_DIR)/input/connect/*.c) \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/string/stdstring.c \
	$(LRC)/rthreads/rthreads.c \
	$(LRC)/rthreads/retro_eventcount.c \
	$(LRC)/features/features_cpu.c \
	$(LRC)/encodings/encoding_utf.c \
	$(LRC)/encodings/encoding_crc32.c

LIB_OBJS := $(call sample_objs,$(LIB_SOURCES:.c=.o))

LIB_CFLAGS := -Wall -std=gnu99 -g1 -O1 -D_GNU_SOURCE \
              -DHAVE_HID -DHAVE_THREADS -DRARCH_INTERNAL \
              -I$(RARCH_DIR) -I$(RARCH_DIR)/input -I$(LRC)/include
TEST_CFLAGS := -Wall -std=gnu89 -Werror=declaration-after-statement \
               -Werror=implicit-function-declaration -g1 -O1 -D_GNU_SOURCE \
               -DHAVE_HID -DHAVE_BTSTACK -DHAVE_DYLIB -DHAVE_THREADS \
               -DRARCH_INTERNAL -I$(RARCH_DIR) -I$(RARCH_DIR)/input -I$(LRC)/include \
               -I$(RARCH_DIR)/tools/platform_stubs/apple
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   LIB_CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(LIB_CFLAGS)
   TEST_CFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(TEST_CFLAGS)
   LDFLAGS     := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

TARGETS := btstack_hid_test btstack_hid_posix_test
DEPS    := btstack_hid_test.c $(RARCH_DIR)/input/drivers_hid/btstack_hid.c \
           cf_sdk/CoreFoundation/CFRunLoop.h

all: $(TARGETS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(LIB_CFLAGS)

btstack_hid_test.o: $(DEPS)
	$(CC) -c -o $@ btstack_hid_test.c $(TEST_CFLAGS) -D__APPLE__ -Icf_sdk

btstack_hid_posix_test.o: $(DEPS)
	$(CC) -c -o $@ btstack_hid_test.c $(TEST_CFLAGS)

btstack_hid_test: btstack_hid_test.o $(LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

btstack_hid_posix_test: btstack_hid_posix_test.o $(LIB_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./btstack_hid_test
	./btstack_hid_posix_test

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGETS) btstack_hid_test.o btstack_hid_posix_test.o

.PHONY: all check clean
