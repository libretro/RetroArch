TARGET := turbo_hold_test

CORE_DIR = ../../..
CC ?= gcc

# input_turbo_hold_apply() is extracted from input_driver.c between its
# BEGIN/END markers, so the test runs the shipping code.
CFLAGS += -std=gnu99 -O1 -g1 -Wall -Werror=implicit-function-declaration \
	-I$(CORE_DIR)/libretro-common/include -I$(CORE_DIR) -I$(CORE_DIR)/deps \
	-DRARCH_INTERNAL

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

turbo_hold_fragment.c: $(CORE_DIR)/input/input_driver.c
	sed -n '/===== BEGIN input turbo and hold =====/,/===== END input turbo and hold =====/p' $< > $@

$(TARGET): turbo_hold_test.c turbo_hold_fragment.c
	$(CC) $(CFLAGS) -o $@ turbo_hold_test.c $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe turbo_hold_fragment.c

.PHONY: all check clean
