# link/link_bus.c alone: one check per rule of the link bus.
# Built as C89, pedantic, as libretro-common is.

CORE_DIR  := ../../..
EXE       ?= $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe,)
TARGET    = link_bus_test$(EXE)

include ../../sample_objdir.mk

SOURCES := link_bus_test.c $(CORE_DIR)/link/link_bus.c
CFLAGS  += -std=c89 -pedantic -Wall -Wextra -Werror -Wdeclaration-after-statement -g1 -O2 -I$(CORE_DIR)/include
OBJS    := $(call sample_objs,$(SOURCES:.c=.o))
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
check: $(TARGET)
	$(RUNNER) ./$(TARGET)
clean:
	rm -f $(TARGET) $(OBJS)
	obj='$(SAMPLE_OBJDIR)'; rm -rf "$${obj:?}"
.PHONY: all check clean
