# os/wiiu/system/newlib_locks.c, the newlib lock hooks on Cafe OS mutexes,
# against stand-in OSMutex functions. The Wii U headers are the tree's
# own (os/wiiu/include). Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET   := wiiu_newlib_locks_test$(EXE)
CORE_DIR := ../../..

CFLAGS  += -std=gnu89 -Wall -g1 -O1 -I$(CORE_DIR)/os/wiiu/include \
           -I$(CORE_DIR)/libretro-common/include
LDFLAGS += -lpthread

# The lock word is an int holding a mutex address, as on the Wii U, so
# the test's static pool must sit below 2^31.
ifeq ($(EXE),.exe)
   LDFLAGS += -static -Wl,--image-base=0x10000000 -Wl,--disable-dynamicbase \
              -Wl,--disable-high-entropy-va
else
   CFLAGS  += -fno-pie
   LDFLAGS += -no-pie
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): wiiu_newlib_locks_test.c $(CORE_DIR)/os/wiiu/system/newlib_locks.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# The same checks against the hooks newlib before 4.3 calls, the set
# the Wii U toolchain's newlib 4.2 uses.
TARGET_PRE43 := wiiu_newlib_locks_pre43_test$(EXE)

$(TARGET_PRE43): wiiu_newlib_locks_test.c $(CORE_DIR)/os/wiiu/system/newlib_locks.c
	$(CC) $(CFLAGS) -D__NEWLIB__=4 -D__NEWLIB_MINOR__=2 -o $@ $< $(LDFLAGS)

check: all $(TARGET_PRE43)
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(TARGET_PRE43)

clean:
	rm -f wiiu_newlib_locks_test wiiu_newlib_locks_test.exe \
	      wiiu_newlib_locks_pre43_test wiiu_newlib_locks_pre43_test.exe

.PHONY: all check clean
