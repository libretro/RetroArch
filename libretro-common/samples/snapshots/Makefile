CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g
BUILD ?= build
CHECK_FLAGS = -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -Werror=frame-larger-than=2048
ifeq ($(OS),Windows_NT)
EXE = .exe
LIB = .dll
else
LIB = .so
LDLIBS += -ldl
endif

all: $(BUILD)/snapshot_smoke$(EXE)

$(BUILD)/snapshot_smoke$(EXE): snapshot_smoke.c ../../include/libretro.h
	mkdir -p "$(BUILD)"
	$(CC) $(CFLAGS) $(CHECK_FLAGS) -I../../include $< -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/snapshot_proxy$(LIB): snapshot_proxy.c snapshot_smoke.c ../../include/libretro.h
	mkdir -p "$(BUILD)"
	$(CC) $(CFLAGS) $(CHECK_FLAGS) -shared -fPIC -I../../include $< -o $@ $(LDFLAGS) $(LDLIBS)

gpu: $(BUILD)/snapshot_proxy$(LIB)

check-fetcher:
	"$(PYTHON)" test_fetch_harness.py

check-build:
	"$(PYTHON)" test_build_harness.py --make "$(MAKE)" --cc "$(CC)" --shell "$(SHELL)"

check: check-build check-fetcher

clean:
	$(RM) "$(BUILD)/snapshot_smoke$(EXE)" "$(BUILD)/snapshot_proxy$(LIB)"

# BUILD may contain binaries from another cached revision or local checkout.
.PHONY: $(BUILD)/snapshot_smoke$(EXE) $(BUILD)/snapshot_proxy$(LIB)
.PHONY: all gpu check check-fetcher check-build clean
