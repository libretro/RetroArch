TARGET := win32_power_plan_test
GEN    := power_plan_sim.c

LIBRETRO_COMM_DIR := ../../../libretro-common
PLATFORM_SRC      ?= ../../../frontend/drivers/platform_win32.c

# The low-latency power plan as it ships, lifted out of platform_win32.c
# and compiled against a fake powrprof instead of <windows.h>.
SOURCES := win32_power_plan_test.c
OBJS    := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=c89 -pedantic -g -O1 \
          -I. -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

# From the powrprof typedefs through frontend_win32_set_power_plan().
$(GEN): $(PLATFORM_SRC)
	awk '/^typedef DWORD \(WINAPI \*PowerGetActiveScheme_t\)/ { on = 1 } \
	     on { print } \
	     on && /^static bool frontend_win32_set_power_plan\(bool on\)$$/ { last = 1 } \
	     last && /^}$$/ { exit }' $< > $@
	grep -q 'win32_power_plan_apply' $@
	grep -q 'frontend_win32_set_power_plan' $@

win32_power_plan_test.o: $(GEN) fake_powrprof.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET) capped
	./$(TARGET) recover
	./$(TARGET) switched
	./$(TARGET) refused
	./$(TARGET) none

clean:
	rm -f $(OBJS) $(TARGET) $(GEN)

.PHONY: all check clean
