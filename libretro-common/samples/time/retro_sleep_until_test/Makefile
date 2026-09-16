CORE_DIR := ../../..
TARGET   := retro_sleep_until_test
SOURCES_C := retro_sleep_until_test.c \
	$(CORE_DIR)/time/rtime.c \
	$(CORE_DIR)/features/features_cpu.c \
	$(CORE_DIR)/string/stdstring.c \
	$(CORE_DIR)/compat/compat_strl.c \
	$(CORE_DIR)/encodings/encoding_utf.c
OBJS := $(SOURCES_C:.c=.o)
CFLAGS  += -Wall -I$(CORE_DIR)/include -D_GNU_SOURCE -O1 -g
LDFLAGS += -lm -lpthread
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
clean:
	rm -f $(OBJS) $(TARGET)
.PHONY: all clean
