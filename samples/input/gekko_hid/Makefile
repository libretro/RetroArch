# The Wii's USB HID driver (input/drivers_hid/gekko_hid.c) taking pads
# away, with os/gekko's threads and USB calls stood in for on pthreads.
# The os/gekko headers are the tree's own. The test includes the
# driver's source itself. Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
#
# check also builds the driver asking a reader to stop only once, which
# must fail.
TARGET        := gekko_hid_test$(EXE)
TARGET_BROKEN := gekko_hid_broken_test$(EXE)

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu89 -g1 -O1 -DRARCH_INTERNAL -DGEKKO -DHW_RVL \
           -DHAVE_HID -I$(RARCH_DIR) -I$(RARCH_DIR)/os/gekko/include \
           -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread

ifeq ($(EXE),.exe)
   LDFLAGS += -static
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

DEPS := gekko_hid_test.c $(RARCH_DIR)/input/drivers_hid/gekko_hid.c

all: $(TARGET) $(TARGET_BROKEN)

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET_BROKEN): $(DEPS)
	$(CC) $(CFLAGS) -DGEKKO_HID_TEST_NO_RECANCEL -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)
	@if $(RUNNER) ./$(TARGET_BROKEN) >/dev/null 2>&1; then \
	   echo "FAIL: the test passed with a reader asked to stop only once"; \
	   exit 1; \
	 else \
	   echo "gekko_hid: asking only once failed, as it must"; \
	 fi

clean:
	rm -f gekko_hid_test gekko_hid_test.exe \
	      gekko_hid_broken_test gekko_hid_broken_test.exe

.PHONY: all check clean
