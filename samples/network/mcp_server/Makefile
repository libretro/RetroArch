# Regression test for the embedded MCP server, network/mcp_server.c,
# against a stub command table. Builds with gcc/clang and with
# mingw-w64 (compiler=x86_64-w64-mingw32-gcc; run the .exe under Wine).
compiler   ?= gcc
TARGET     := mcp_server_test
CORE_DIR    = ../../..
LIBRETRO_COMM_DIR = $(CORE_DIR)/libretro-common

SOURCES := \
	mcp_server_test.c \
	stubs.c \
	$(CORE_DIR)/network/mcp_server.c \
	$(LIBRETRO_COMM_DIR)/net/net_socket.c \
	$(LIBRETRO_COMM_DIR)/net/net_compat.c \
	$(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS += -std=gnu99 -Wall -g -O1 -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) \
	-DHAVE_NETWORKING -DHAVE_COMMAND -DHAVE_NETWORK_CMD -DHAVE_MCP -DRARCH_INTERNAL
ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifneq ($(findstring mingw,$(compiler)),)
   TARGET  := $(TARGET).exe
   LDFLAGS += -lws2_32 -static
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(compiler) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f mcp_server_test mcp_server_test.exe

.PHONY: all check clean
