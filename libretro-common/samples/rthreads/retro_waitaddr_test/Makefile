include ../../sample_objdir.mk

TARGET := retro_waitaddr_test

LIBRETRO_COMM_DIR := ../../..

# The real primitive out of the tree, over the real eventcount.
SOURCES := \
	retro_waitaddr_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_waitaddr.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

CFLAGS  += -Wall -W -std=gnu99 -g1 -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

# SCOND=1 runs it over the eventcount's condition-variable backend, the
# one most targets take.
ifneq ($(SCOND),)
   CFLAGS += -DRETRO_EVENTCOUNT_FORCE_SCOND
   TARGET := retro_waitaddr_test_scond
endif

.SECONDEXPANSION:

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

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

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./$(TARGET)
	@$(MAKE) -s clean
	@$(MAKE) -s SCOND=1 $(if $(SANITIZER),SANITIZER=$(SANITIZER))
	./retro_waitaddr_test_scond
	@$(MAKE) -s clean

clean:
	rm -rf $(SAMPLE_OBJDIR) *.o retro_waitaddr_test retro_waitaddr_test_scond

.PHONY: all check clean
