STATIC_LINKING	:= 0
AR				:= ar
DEBUG			:= 0
TARGET_NAME		:= mpv
PREFIX			:= /usr
LIBDIR			:= $(PREFIX)/lib
LIBRETRO_DIR	:= libretro

ifeq ($(platform),)
   platform = unix
   ifeq ($(shell uname -s),)
      EXE_EXT = .exe
      platform = win
   else ifneq ($(findstring MINGW,$(shell uname -s)),)
      platform = win
   else ifneq ($(findstring Darwin,$(shell uname -s)),)
      platform = osx
      ifeq ($(shell uname -p),powerpc)
         arch = ppc
      else
         arch = intel
      endif
   else ifneq ($(findstring win,$(shell uname -s)),)
      platform = win
   endif
else ifneq (,$(findstring armv,$(platform)))
   override platform += unix
else ifneq (,$(findstring rpi3,$(platform)))
   override platform += unix
endif

ifeq ($(ARCHFLAGS),)
ifeq ($(archs),ppc)
	ARCHFLAGS = -arch ppc -arch ppc64
else
	ARCHFLAGS = -arch i386 -arch x86_64
endif
endif

ifeq ($(platform), osx)
ifndef ($(NOUNIVERSAL))
	CFLAGS += $(ARCHFLAGS)
	LFLAGS += $(ARCHFLAGS)
endif
endif

ifeq ($(STATIC_LINKING), 1)
	EXT := a
endif

ifneq (,$(findstring unix,$(platform)))
	EXT ?= so
	TARGET := $(TARGET_NAME)_libretro.$(EXT)
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else ifneq (,$(findstring osx,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.dylib
	fpic := -fPIC
	SHARED := -dynamiclib
else ifneq (,$(findstring ios,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_ios.dylib
	fpic := -fPIC
	SHARED := -dynamiclib
ifeq ($(IOSSDK),)
	IOSSDK := $(shell xcodebuild -version -sdk iphoneos Path)
endif
	DEFINES := -DIOS
	CC = cc -arch armv7 -isysroot $(IOSSDK)
ifeq ($(platform),ios9)
CC     += -miphoneos-version-min=8.0
CFLAGS += -miphoneos-version-min=8.0
else
CC     += -miphoneos-version-min=5.0
CFLAGS += -miphoneos-version-min=5.0
endif #ifneq ios
else ifneq (,$(findstring qnx,$(platform)))
	TARGET := $(TARGET_NAME)_libretro_qnx.so
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else ifeq ($(platform), emscripten)
	TARGET := $(TARGET_NAME)_libretro_emscripten.bc
	fpic := -fPIC
	SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else ifeq ($(platform), vita)
	TARGET := $(TARGET_NAME)_vita.a
	CC = arm-vita-eabi-gcc
	AR = arm-vita-eabi-ar
	CFLAGS += -Wl,-q -Wall -O3
	STATIC_LINKING = 1
else
	CC = gcc
	TARGET := $(TARGET_NAME)_libretro.dll
	SHARED := -shared -static-libgcc -static-libstdc++ \
		-Wl,--version-script=link.T -Wl,--no-undefined
endif

ifneq (,$(findstring gles,$(platform)))
   CFLAGS += -DHAVE_OPENGLES
endif

# Locale support should be enabled by default, as it's most common.
ifneq ($(locale),false)
	CFLAGS += -DHAVE_LOCALE
endif

# Always have some debugging information.
CFLAGS += -g
ifeq ($(DEBUG), 1)
   CFLAGS += -Og
else
   CFLAGS += -Ofast
endif

CFLAGS   += -I../../libretro-common/include

OBJECTS	:= mpv-libretro.o
LDFLAGS	+= -lmpv -lm
CFLAGS	+= -Wall -pedantic

all: $(TARGET)

$(TARGET): $(OBJECTS)
ifeq ($(STATIC_LINKING), 1)
	$(AR) rcs $@ $(OBJECTS)
else
	$(CC) $(fpic) $(SHARED) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)
endif

%.o: %.c
	$(CC) $(CFLAGS) $(fpic) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

install:
	install -D -m 755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(LIBRETRO_DIR)/$(TARGET)

install-snip:
	install -D -s -m 755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(LIBRETRO_DIR)/$(TARGET)

uninstall:
	rm $(DESTDIR)$(LIBDIR)/$(LIBRETRO_DIR)/$(TARGET)

help:
	@echo 'make <target> [flags]'
	@echo 'Targets:'
	@echo ' clean install install-snip uninstall help'
	@echo 'Flags:'
	@echo ' platform	String containing platform details.'
	@echo ' locale		Support toolchain with locale support.'
	@echo '       		false: disable support, otherwise enabled (default).'

.PHONY: clean
