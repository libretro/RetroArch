# link/link_sched.c over link/link_bus.c, with fake consoles on libco
# cothreads. Built as C89, pedantic, as libretro-common is; libco.c with the
# flags of its own sample (libco/context), which it needs.

CORE_DIR  := ../../..
EXE       ?= $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe,)
TARGET    = link_sched_test$(EXE)

include ../../sample_objdir.mk

SOURCES      := link_sched_test.c $(CORE_DIR)/link/link_sched.c $(CORE_DIR)/link/link_bus.c
LIBCO        := $(call sample_objs,$(CORE_DIR)/libco/libco.o)
CFLAGS       += -std=c89 -pedantic -Wall -Wextra -Werror -Wdeclaration-after-statement -g1 -O2 -I$(CORE_DIR)/include
LIBCO_CFLAGS := -std=c89 -Wall -Wextra -Werror -D_GNU_SOURCE -DHAVE_POSIX_MEMALIGN=1 -g1 -O2 -I$(CORE_DIR)/include
OBJS         := $(call sample_objs,$(SOURCES:.c=.o)) $(LIBCO)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -DLINK_SCHED_TEST_STACK=8388608 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
$(LIBCO): $(CORE_DIR)/libco/libco.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(LIBCO_CFLAGS)
$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	$(RUNNER) ./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
	obj='$(SAMPLE_OBJDIR)'; rm -rf "$${obj:?}"
.PHONY: all check clean
