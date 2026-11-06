ROOT := ../../..
BUILD_DIR ?= .
THREADS ?= 1
PYTHON ?= python3
MUTANT ?=
MUTANTS := missing-release overwrite-on-refusal missing-halt-message \
           late-registration missing-preempt-release stale-history
TRIPLE := $(shell $(CC) -dumpmachine)
EXE :=
SO := .so
SHARED := -shared -fPIC
ifneq ($(findstring mingw,$(TRIPLE)),)
EXE := .exe
SO := .dll
SHARED := -shared
else ifeq ($(shell uname -s),Darwin)
SO := .dylib
SHARED := -dynamiclib -fPIC
endif
TARGET := $(BUILD_DIR)/lifetime_test$(EXE)
TEST_SOURCE := lifetime_test.c
ifneq ($(MUTANT),)
ifeq ($(filter $(MUTANT),$(MUTANTS)),)
$(error Unknown MUTANT: $(MUTANT))
endif
TARGET := $(BUILD_DIR)/mutants/$(MUTANT)/lifetime_test$(EXE)
TEST_SOURCE := $(BUILD_DIR)/mutants/$(MUTANT)/lifetime_test.c
endif
CORE := $(BUILD_DIR)/lifetime_libretro$(SO)
CFLAGS += -O2 -g1 -std=gnu89 -Wall -Werror=declaration-after-statement \
          -I$(ROOT) -I$(ROOT)/deps -I$(ROOT)/libretro-common/include
TEST_FLAGS := -DHAVE_RUNAHEAD -ffunction-sections -fdata-sections -flto
ifeq ($(THREADS),1)
TEST_FLAGS += -DHAVE_THREADS
endif
TEST_LDFLAGS := -flto -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(CORE)

ifneq ($(MUTANT),)
$(TEST_SOURCE): mutants.py lifetime_test.c $(ROOT)/runahead.c
	$(PYTHON) mutants.py generate $(MUTANT) "$@"
endif

$(BUILD_DIR):
	mkdir -p $@

$(TARGET): $(TEST_SOURCE) reference_core.c reference_core.h $(ROOT)/runahead.c $(ROOT)/runahead.h $(ROOT)/runloop.h $(ROOT)/core.h $(ROOT)/command.h $(ROOT)/libretro-common/include/libretro.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(TEST_FLAGS) "$(TEST_SOURCE)" reference_core.c -o $@ $(LDFLAGS) $(TEST_LDFLAGS)

$(CORE): reference_core.c reference_core.h $(ROOT)/libretro-common/include/libretro.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SHARED) reference_core.c -o $@ $(LDFLAGS)

check: all
ifneq ($(MUTANT),)
	$(MAKE) check MUTANT= FRONTEND=
	$(PYTHON) mutants.py check $(MUTANT) "$(abspath $(TARGET))"
else
	$(TARGET)
ifneq ($(FRONTEND),)
	$(PYTHON) test.py --frontend "$(FRONTEND)" --core "$(abspath $(CORE))" $(TEST_ARGS)
endif
endif

mutants:
	$(MAKE) check MUTANT=missing-release
	$(MAKE) check MUTANT=overwrite-on-refusal
	$(MAKE) check MUTANT=missing-halt-message
	$(MAKE) check MUTANT=late-registration
	$(MAKE) check MUTANT=missing-preempt-release
	$(MAKE) check MUTANT=stale-history

clean:
	rm -f $(TARGET) $(CORE)

.PHONY: all check clean mutants
