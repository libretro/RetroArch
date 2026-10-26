# os/wiiu/system/exception_handler.c against stand-in coreinit functions;
# the Wii U headers are the tree's own (os/wiiu/include). Builds for Linux
# and, with CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET   := wiiu_exception_cores_test$(EXE)
CORE_DIR := ../../..

# The handler stores Wii U addresses in 32-bit words; on a 64-bit host
# those casts only narrow the values the test never dereferences.
CFLAGS  += -std=gnu89 -Wall -g1 -O1 -I$(CORE_DIR) -I$(CORE_DIR)/os/wiiu \
           -I$(CORE_DIR)/os/wiiu/include -I$(CORE_DIR)/libretro-common/include \
           -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): wiiu_exception_cores_test.c $(CORE_DIR)/os/wiiu/system/exception_handler.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f wiiu_exception_cores_test wiiu_exception_cores_test.exe

.PHONY: all check clean
