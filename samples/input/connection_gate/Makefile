TARGET := connection_gate_test
REPO_ROOT := ../../..
LRC := $(REPO_ROOT)/libretro-common
CFLAGS += -Wall -std=gnu89 -g -O1 -DHAVE_THREADS -DHAVE_HID \
   -I$(REPO_ROOT) -I$(REPO_ROOT)/input -I$(LRC)/include
LDLIBS += -lpthread -lm
SOURCES := connection_gate_test.c $(LRC)/rthreads/rthreads.c \
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
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean
