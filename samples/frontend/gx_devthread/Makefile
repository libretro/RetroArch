# The Wii's device thread in frontend/drivers/platform_gx_libogc.c, with
# libogc's disc interfaces and libfat stood in for (stubs/). The test
# includes the frontend's source itself; whole-program LTO drops the
# parts of it that would reach the rest of RetroArch. Builds for Linux
# and, with CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
#
# check also builds a stop that does not wake the thread, which must
# fail.
TARGET        := gx_devthread_test$(EXE)
TARGET_BROKEN := gx_devthread_broken_test$(EXE)

RARCH_DIR := ../../..
LRC       := $(RARCH_DIR)/libretro-common

SOURCES := gx_devthread_test.c \
           $(LRC)/rthreads/rthreads.c \
           $(LRC)/rthreads/retro_eventcount.c \
           $(LRC)/features/features_cpu.c \
           $(LRC)/time/rtime.c

CFLAGS  += -std=gnu99 -Wall -g1 -O1 -flto -fwhole-program \
           -ffunction-sections -fdata-sections \
           -DHAVE_THREADS -DRARCH_INTERNAL -Wno-unused-result \
           -Istubs -I$(RARCH_DIR) -I$(LRC)/include
LDFLAGS += -flto -Wl,--gc-sections -lpthread

ifeq ($(EXE),.exe)
   LDFLAGS += -static
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

DEPS := $(SOURCES) $(RARCH_DIR)/frontend/drivers/platform_gx_libogc.c

all: $(TARGET) $(TARGET_BROKEN)

$(TARGET): $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_BROKEN): $(DEPS)
	$(CC) $(CFLAGS) -DGX_DEVTHREAD_TEST_NO_WAKE -o $@ $(SOURCES) $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)
	@if $(RUNNER) ./$(TARGET_BROKEN) >/dev/null 2>&1; then \
	   echo "FAIL: the test passed with a stop that does not wake the thread"; \
	   exit 1; \
	 else \
	   echo "gx_devthread: an unwoken stop failed, as it must"; \
	 fi

clean:
	rm -f gx_devthread_test gx_devthread_test.exe \
	      gx_devthread_broken_test gx_devthread_broken_test.exe

.PHONY: all check clean
