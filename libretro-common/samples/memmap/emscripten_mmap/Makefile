LIBRETRO_COMM_DIR := ../../..
TARGET := emscripten_mmap_test
WASM_TARGET := emscripten_mmap.js
EMCC ?= emcc
NODE ?= node

SOURCES := emscripten_mmap.c $(LIBRETRO_COMM_DIR)/memmap/memmap.c
CFLAGS += -std=c89 -D_GNU_SOURCE -Wall -Wextra -Werror -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(LIBRETRO_COMM_DIR)/include/memmap.h
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

$(WASM_TARGET): $(SOURCES) $(LIBRETRO_COMM_DIR)/include/memmap.h
	$(EMCC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -sENVIRONMENT=node -o $@

check: $(TARGET)
	./$(TARGET)

check-wasm: $(WASM_TARGET)
	$(NODE) $(WASM_TARGET)

clean:
	rm -f $(TARGET) $(WASM_TARGET) emscripten_mmap.wasm

.PHONY: all check check-wasm clean
