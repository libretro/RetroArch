ROOT := ../../..
BUILD_DIR ?= .
THREADS ?= 1
PYTHON ?= python3
TRIPLE := $(shell $(CC) -dumpmachine)
EXE :=
SO := .so
SHARED := -shared -fPIC
CORE_LIBS := -pthread
ifneq ($(findstring mingw,$(TRIPLE)),)
EXE := .exe
SO := .dll
SHARED := -shared
CORE_LIBS :=
else ifeq ($(shell uname -s),Darwin)
SO := .dylib
SHARED := -dynamiclib -fPIC
endif
TARGET := $(BUILD_DIR)/lifetime_test$(EXE)
TEST_SOURCE := lifetime_test.c
CORE := $(BUILD_DIR)/lifetime_libretro$(SO)
CFLAGS += -O2 -g1 -std=gnu89 -Wall -Werror=declaration-after-statement \
          -I$(ROOT) -I$(ROOT)/deps -I$(ROOT)/libretro-common/include
# The test includes all of runahead.c; the linker drops what it does not call.
TEST_FLAGS := -DHAVE_RUNAHEAD -ffunction-sections -fdata-sections
ifeq ($(THREADS),1)
TEST_FLAGS += -DHAVE_THREADS
endif
TEST_LDFLAGS := -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(CORE)

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): $(TEST_SOURCE) reference_core.c reference_core.h $(ROOT)/runahead.c $(ROOT)/runahead.h $(ROOT)/runloop.h $(ROOT)/core.h $(ROOT)/command.h $(ROOT)/libretro-common/include/libretro.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_FLAGS) "$(TEST_SOURCE)" reference_core.c -o $@ $(LDFLAGS) $(TEST_LDFLAGS) $(CORE_LIBS)

$(CORE): reference_core.c reference_core.h $(ROOT)/libretro-common/include/libretro.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SHARED) reference_core.c -o $@ $(LDFLAGS) $(CORE_LIBS)

check: all
	$(TARGET)
ifneq ($(FRONTEND),)
	$(PYTHON) test.py --frontend "$(FRONTEND)" --core "$(abspath $(CORE))" $(TEST_ARGS)
endif

clean:
	rm -f $(TARGET) $(CORE)

.PHONY: all check clean
