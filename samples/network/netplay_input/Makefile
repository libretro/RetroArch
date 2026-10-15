TARGET := netplay_input_test

CORE_DIR = ../../..
INCDIRS  := -I$(CORE_DIR)/libretro-common/include -I$(CORE_DIR) -I$(CORE_DIR)/deps

CC ?= gcc

# The two helpers are extracted from netplay_frontend.c between their
# BEGIN/END markers, so the test runs the shipping code.
CFLAGS += -std=gnu99 -O1 -g1 -Wall -Werror=implicit-function-declaration \
	$(INCDIRS) -DRARCH_INTERNAL -DHAVE_NETWORKING

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

FRAGMENTS := real_clients_fragment.c input_decode_fragment.c

all: $(TARGET)

real_clients_fragment.c: $(CORE_DIR)/network/netplay/netplay_frontend.c
	sed -n '/===== BEGIN netplay real clients =====/,/===== END netplay real clients =====/p' $< > $@

input_decode_fragment.c: $(CORE_DIR)/network/netplay/netplay_frontend.c
	sed -n '/===== BEGIN netplay input decode =====/,/===== END netplay input decode =====/p' $< > $@

$(TARGET): netplay_input_test.c $(FRAGMENTS)
	$(CC) $(CFLAGS) -o $@ netplay_input_test.c $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(FRAGMENTS)

.PHONY: all check clean
