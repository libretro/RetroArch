TARGET := connection_gate_test
REPO_ROOT := ../../..
LRC := $(REPO_ROOT)/libretro-common
CFLAGS += -Wall -std=gnu89 -g1 -O1 -DHAVE_THREADS -DHAVE_HID \
   -I$(REPO_ROOT) -I$(REPO_ROOT)/input -I$(LRC)/include
LDLIBS += -lpthread -lm
SOURCES := connection_gate_test.c $(LRC)/rthreads/rthreads.c \
   $(LRC)/rthreads/retro_eventcount.c \
   $(LRC)/features/features_cpu.c $(LRC)/string/stdstring.c \
   $(LRC)/compat/compat_strl.c $(LRC)/compat/compat_strcasestr.c \
   $(LRC)/encodings/encoding_utf.c $(LRC)/time/rtime.c
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDLIBS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/input/connect/joypad_connection.c \
   $(REPO_ROOT)/input/connect/joypad_connection.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)
connection_gate_spin_test: $(SOURCES) $(REPO_ROOT)/input/connect/joypad_connection.c \
   $(REPO_ROOT)/input/connect/joypad_connection.h
	$(CC) $(CFLAGS) -DCONNECTION_GATE_TEST_NO_PARK -o $@ $(SOURCES) $(LDLIBS)
check: all connection_gate_spin_test
	./$(TARGET)
	@if ./connection_gate_spin_test >/dev/null 2>&1; then \
	   echo "FAIL: a retirement that spins passed"; exit 1; \
	 else \
	   echo "connection_gate: a spinning retirement failed, as it must"; \
	 fi
clean:
	rm -f $(TARGET) connection_gate_spin_test
.PHONY: all check clean
