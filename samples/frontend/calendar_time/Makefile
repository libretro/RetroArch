# calendar_time.c: the snapshot and callbacks behind
# RETRO_ENVIRONMENT_GET_CALENDAR_TIME_INTERFACE. Built natively and, where
# mingw-w64 and Wine are installed, as a Windows build run under Wine.
TARGET := calendar_time_test
ROOT   := ../../..
LIBRETRO_COMM_DIR := $(ROOT)/libretro-common

MINGW_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
WINE     := $(shell command -v wine 2>/dev/null)

SOURCES := calendar_time_test.c \
	$(ROOT)/calendar_time.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

TEST_CFLAGS := -std=gnu89 -Wall -g1 -O1 -DHAVE_THREADS \
               -I$(ROOT) -I$(LIBRETRO_COMM_DIR)/include
CFLAGS  += $(TEST_CFLAGS)
LDFLAGS += -lpthread
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

ifneq ($(MINGW_CC),)
ifneq ($(WINE),)
WIN_TARGET := $(TARGET).exe
endif
endif

all: $(TARGET) $(WIN_TARGET)

$(TARGET): $(SOURCES) $(ROOT)/calendar_time.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

# No sanitizer: mingw-w64 has none
$(TARGET).exe: $(SOURCES) $(ROOT)/calendar_time.h
	$(MINGW_CC) $(TEST_CFLAGS) $(SOURCES) -o $@

check: all
	./$(TARGET)
ifeq ($(WIN_TARGET),)
	@echo "$(TARGET).exe: no x86_64-w64-mingw32-gcc or wine, skipped"
else
	WINEDEBUG=-all $(WINE) ./$(WIN_TARGET)
endif

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean
