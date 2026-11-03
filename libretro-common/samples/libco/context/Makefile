CC ?= cc
CPPFLAGS += -I../../../include -DLIBCO_MP -D_GNU_SOURCE -DHAVE_POSIX_MEMALIGN=1
CFLAGS ?= -O2
CFLAGS += -std=c89 -Wall -Wextra -Werror
BACKEND ?= ../../../libco/libco.c
EXE ?= $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),.exe,)
TARGET = context_test$(EXE)

.PHONY: all check clean
all: $(TARGET)
$(TARGET): context_test.c context_probe.S $(BACKEND) ../../../libco/amd64.c ../../../libco/aarch64.c ../../../include/libco.h
	$(CC) $(CPPFLAGS) $(CFLAGS) context_test.c context_probe.S $(BACKEND) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	$(RUNNER) ./$(TARGET)
clean:
	$(RM) $(TARGET)
