OBJDIR ?= .
EXE_EXT := $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe)
TARGET := $(OBJDIR)/psl1ght_mutex_test$(EXE_EXT)
CFLAGS += -std=gnu99 -O2 -Wall -Wextra -Werror -Iinclude
CFLAGS += -I../../../deps/rcheevos/include

all: $(TARGET)

$(TARGET): psl1ght_mutex_test.c include/lv2/mutex.h ../../../deps/rcheevos/src/rc_compat.c ../../../deps/rcheevos/src/rc_compat.h
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) psl1ght_mutex_test.c $(LDFLAGS) -o $@

check: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: all check clean
