TARGET := memshm_area_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := memshm_area_test.c $(LIBRETRO_COMM_DIR)/memmap/memmap.c
CFLAGS  += -Wall -W -std=gnu99 -g -O1 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lrt
OBJS := $(SOURCES:.c=.o)
ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
# On Windows the legacy slot-reservation path is what every system
# before 10 1803 uses; this builds it on any system so it can be run.
legacy: $(SOURCES)
	$(CC) -o $(TARGET)_legacy $(SOURCES) $(CFLAGS) -DMEMSHM_AREA_FORCE_LEGACY $(LDFLAGS)

check: all
	./$(TARGET)
clean:
	rm -f $(OBJS) $(TARGET) $(TARGET)_legacy
.PHONY: all check clean legacy
