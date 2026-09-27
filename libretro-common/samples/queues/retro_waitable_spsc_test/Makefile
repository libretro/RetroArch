TARGET := retro_waitable_spsc_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	retro_waitable_spsc_test.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_waitable_spsc.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

CFLAGS  += -Wall -W -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

# SCOND=1 forces the eventcount's condition-variable backend, which is
# what macOS, the BSDs and the console ports get.
ifneq ($(SCOND),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SCOND
   TARGET := retro_waitable_spsc_test_scond
endif

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

check: all
	./$(TARGET)
	@$(MAKE) -s clean
	@$(MAKE) -s SCOND=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_waitable_spsc_test_scond

clean:
	rm -f $(OBJS) retro_waitable_spsc_test retro_waitable_spsc_test_scond

.PHONY: all check clean
