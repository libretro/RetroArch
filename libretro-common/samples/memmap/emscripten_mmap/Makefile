LIBRETRO_COMM_DIR := ../../..
TARGET := emscripten_mmap_test
WASM_TARGET := emscripten_mmap.js
EMCC ?= emcc

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

# The first node that actually runs, tried in order: one the caller
# named, the one emsdk exports, the one emcc records in its config,
# then whatever is on the PATH. A name on the PATH is not enough - on
# some runners it is there and not executable, and the build then
# succeeds and the run dies with "Permission denied" - so each
# candidate is asked for its version before it is used.
check-wasm: $(WASM_TARGET)
	@node=""; \
	for c in "$(NODE)" "$$EMSDK_NODE" \
	         "`$(EMCC) --em-config 2>/dev/null | sed -n \"s/.*NODE_JS *= *['\\\"]\\([^'\\\"]*\\).*/\\1/p\" | head -n1`" \
	         node nodejs; do \
	   [ -n "$$c" ] || continue; \
	   "$$c" --version >/dev/null 2>&1 || continue; \
	   node="$$c"; break; \
	done; \
	if [ -z "$$node" ]; then \
	   echo "check-wasm: SKIPPED - no node that runs here (tried NODE=$(NODE), EMSDK_NODE, emcc --em-config, node, nodejs)." >&2; \
	   echo "check-wasm: the wasm was built; running it needs a node the environment supplies." >&2; \
	   exit 0; \
	fi; \
	echo "$$node $(WASM_TARGET)"; \
	"$$node" $(WASM_TARGET)

clean:
	rm -f $(TARGET) $(WASM_TARGET) emscripten_mmap.wasm

.PHONY: all check check-wasm clean
