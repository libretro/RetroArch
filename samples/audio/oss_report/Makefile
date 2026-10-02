TARGET := oss_report_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# sys/soundcard.h here is Linux's <linux/soundcard.h> under the name
# the driver includes.
INCDIRS := -I. -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := oss_report_test.o $(LIBRETRO_COMM_DIR)/lists/string_list.o $(LIBRETRO_COMM_DIR)/compat/compat_strl.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The driver is #included by the test.
oss_report_test.o: oss_report_test.c ../../../audio/drivers/oss.c
	$(CC) $(CFLAGS) -Wno-unused-function -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean
