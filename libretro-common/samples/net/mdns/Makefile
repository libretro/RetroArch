include ../../sample_objdir.mk
TARGET := net_mdns_test$(EXE)
COMMON := ../../..
CC ?= cc
OPTIMIZE ?= -O2
CFLAGS += $(OPTIMIZE) -g -std=c89 -pedantic -Wall -Wextra -Werror -Wdeclaration-after-statement -Wframe-larger-than=2048 -DHAVE_MDNS -I$(COMMON)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# boolean.h preserves the shared _Bool ABI in Clang's C89 mode.
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)
$(TARGET): net_mdns_test.c $(COMMON)/net/net_mdns.c $(COMMON)/include/net/net_mdns.h
	$(CC) $(CFLAGS) net_mdns_test.c $(COMMON)/net/net_mdns.c $(LDFLAGS) -o $@
check: $(TARGET)
	$(RUNNER) ./$(TARGET)
TRANSPORT_SRC := $(COMMON)/net/net_mdns_socket.c $(COMMON)/net/net_compat.c $(COMMON)/compat/compat_strl.c

demo: discovery.c $(COMMON)/net/net_mdns.c $(TRANSPORT_SRC)
	@mkdir -p $(SAMPLE_OBJDIR)
	$(CC) $(CFLAGS) discovery.c $(COMMON)/net/net_mdns.c $(TRANSPORT_SRC) $(LDFLAGS) $(if $(filter .exe,$(EXE)),-lws2_32) -o $(SAMPLE_OBJDIR)/mdns_discovery$(EXE)
clean:
	rm -f net_mdns_test net_mdns_test.exe
	rm -rf $(SAMPLE_OBJDIR)
.PHONY: all check clean demo
