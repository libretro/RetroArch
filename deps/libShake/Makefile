ifeq ($(PLATFORM), gcw0)
  CC         := /opt/gcw0-toolchain/usr/bin/mipsel-linux-gcc
  STRIP      := /opt/gcw0-toolchain/usr/bin/mipsel-linux-strip
  BACKEND    := LINUX
endif

ifndef BACKEND
$(error Please specify BACKEND. Possible values: LINUX, OSX")
endif

LIBNAME      := libshake
SOVERSION    := 2

SRCDIRS      := common

ifeq ($(BACKEND), LINUX)
  LIBEXT     := .so
  SONAME     := $(LIBNAME)$(LIBEXT).$(SOVERSION)
  PREFIX     ?= /usr
  LDFLAGS    :=-Wl,-soname,$(SONAME)
  SRCDIRS    += linux
else ifeq ($(BACKEND), OSX)
  LIBEXT     := .dylib
  SONAME     := $(LIBNAME).$(SOVERSION)$(LIBEXT)
  PREFIX     ?= /usr/local
  LDFLAGS    := -Wl,-framework,Cocoa -framework IOKit -framework CoreFoundation -framework ForceFeedback -install_name $(SONAME)
  SRCDIRS    += osx
endif

CC           ?= gcc
STRIP        ?= strip
TARGET       ?= $(SONAME)
SYSROOT      := $(shell $(CC) --print-sysroot)
MACHINE      ?= $(shell $(CC) -dumpmachine)
DESTDIR      ?= $(SYSROOT)
CFLAGS       := -fPIC
SRCDIR       := src
OBJDIR       := obj/$(MACHINE)
SRC          := $(foreach dir,$(SRCDIRS),$(sort $(wildcard $(addprefix $(SRCDIR)/,$(dir))/*.c)))
OBJ          := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRC))

ifdef DEBUG
  CFLAGS += -ggdb -Wall -Werror -pedantic -std=c89
else
  CFLAGS += -O2
endif

.PHONY: all install-headers install-lib install clean

all:	$(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -shared $(CFLAGS) $^ -o $@
ifdef DO_STRIP
	$(STRIP) $@
endif

$(OBJ): $(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@ -I include

install-headers:
	cp include/*.h $(DESTDIR)$(PREFIX)/include/

install-lib:
	cp $(TARGET) $(DESTDIR)$(PREFIX)/lib/
	ln -sf $(TARGET) $(DESTDIR)$(PREFIX)/lib/$(LIBNAME)$(LIBEXT)

install: $(TARGET) install-headers install-lib

clean:
	rm -Rf $(TARGET) $(OBJDIR)
