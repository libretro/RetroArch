# menu/menu_timedate.c: the menu clock's date order, styles, text and
# AM/PM designators. Builds natively, and as a Windows build with
#   make CC=x86_64-w64-mingw32-gcc EXE=.exe
TARGET := timedate_test
ROOT   := ../../..
LIBRETRO_COMM_DIR := $(ROOT)/libretro-common
EXE    ?=

SOURCES := timedate_test.c \
	$(ROOT)/menu/menu_timedate.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS += -std=gnu89 -Wall -g1 -O1 -I$(ROOT) -I$(LIBRETRO_COMM_DIR)/include
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)$(EXE)

$(TARGET)$(EXE): $(SOURCES) $(ROOT)/menu/menu_timedate.h $(ROOT)/menu/menu_defines.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: all
	./$(TARGET)$(EXE)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
