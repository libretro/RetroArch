LIBRETRO_COMM_DIR := ../../..
TARGET := emscripten_mmap_test
WASM_TARGET := emscripten_mmap.js
EMCC ?= emcc
# The node the SDK ships, which is the one emcc itself runs its tools
# with. A bare "node" is whatever the PATH has, and on a runner that
# is sometimes a directory entry with no execute bit - the build
# succeeds and the run dies with "Permission denied". emcc prints the
# path it uses; a PATH node is the fallback for a machine without the
# config.
EMSDK_NODE := $(shell $(EMCC) --em-config 2>/dev/null | sed -n 's/^NODE_JS *= *//p' | tr -d "'\"[]" | awk '{print $$1}')
ifneq ($(wildcard $(EMSDK_NODE)),)
NODE ?= $(EMSDK_NODE)
else
NODE ?= node
endif

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
	@command -v $(NODE) >/dev/null 2>&1 || { \
	   echo "check-wasm: no usable node ($(NODE)); set NODE= to one" >&2; exit 1; }
	$(NODE) $(WASM_TARGET)

clean:
	rm -f $(TARGET) $(WASM_TARGET) emscripten_mmap.wasm

.PHONY: all check check-wasm clean
