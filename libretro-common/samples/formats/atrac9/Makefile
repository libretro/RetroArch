include ../../sample_objdir.mk

TARGETS := ratrac9_test ratrac9_vectors_test

LIBRETRO_COMM_DIR := ../../..

OBJS_ratrac9_test := $(call sample_objs,ratrac9_test.o)
OBJS_ratrac9_vectors_test := $(call sample_objs,ratrac9_vectors_test.o \
	$(LIBRETRO_COMM_DIR)/formats/atrac9/ratrac9.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

ratrac9_test: $(OBJS_ratrac9_test)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

ratrac9_vectors_test: $(OBJS_ratrac9_vectors_test)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGETS) $(OBJS_ratrac9_test) $(OBJS_ratrac9_vectors_test)

.PHONY: all clean
