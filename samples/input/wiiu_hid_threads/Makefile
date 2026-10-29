# The Wii U HID driver (input/drivers_hid/wiiu_hid.c) across its
# threads: the HID attach callbacks, the polling thread, the frontend
# and the read completions, with Cafe OS stood in for on pthreads. The
# Wii U headers are the tree's own (os/wiiu/include). The test includes
# the driver's source itself. Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
#
# check also builds the test against an HID stack that loses the reads
# in flight when the client goes, which it must fail.
TARGET        := wiiu_hid_threads_test$(EXE)
TARGET_BROKEN := wiiu_hid_threads_broken_test$(EXE)

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu89 -g1 -O1 -DRARCH_INTERNAL -DWIIU -DHAVE_HID \
           -DWIIU_HID -I$(RARCH_DIR) -I$(RARCH_DIR)/input/include \
           -I$(RARCH_DIR)/os/wiiu/include -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread

ifeq ($(EXE),.exe)
   LDFLAGS += -static
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

DEPS := wiiu_hid_threads_test.c $(RARCH_DIR)/input/drivers_hid/wiiu_hid.c \
        $(RARCH_DIR)/input/include/wiiu/hid.h

all: $(TARGET) $(TARGET_BROKEN)

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET_BROKEN): $(DEPS)
	$(CC) $(CFLAGS) -DWIIU_HID_THREADS_BROKEN_IOS -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)
	@if $(RUNNER) ./$(TARGET_BROKEN) >/dev/null 2>&1; then \
	   echo "FAIL: the test passed against an HID stack losing reads"; \
	   exit 1; \
	 else \
	   echo "wiiu_hid_threads: the broken HID stack failed, as it must"; \
	 fi

clean:
	rm -f wiiu_hid_threads_test wiiu_hid_threads_test.exe \
	      wiiu_hid_threads_broken_test wiiu_hid_threads_broken_test.exe

.PHONY: all check clean
