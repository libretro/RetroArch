CORE_DIR := ../../..
TARGET   := rtime_localtime_test
SOURCES_C := rtime_localtime_test.c \
	$(CORE_DIR)/time/rtime.c \
	$(CORE_DIR)/rthreads/rthreads.c \
	$(CORE_DIR)/features/features_cpu.c \
	$(CORE_DIR)/string/stdstring.c \
	$(CORE_DIR)/compat/compat_strl.c \
	$(CORE_DIR)/encodings/encoding_utf.c
OBJS := $(SOURCES_C:.c=.o)
CFLAGS  += -Wall -I$(CORE_DIR)/include -DHAVE_THREADS -D_GNU_SOURCE -O1 -g
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(TARGET)
.PHONY: all clean
