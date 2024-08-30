PLATFORM := $(shell uname)

MAIN = switchres_main
STANDALONE = switchres
TARGET_LIB = libswitchres
DRMHOOK_LIB = libdrmhook
GRID = grid
SRC = monitor.cpp modeline.cpp switchres.cpp display.cpp custom_video.cpp log.cpp switchres_wrapper.cpp edid.cpp
OBJS = $(SRC:.cpp=.o)

CROSS_COMPILE ?=
CXX ?= g++
AR ?= ar
LDFLAGS = -shared
FINAL_CXX=$(CROSS_COMPILE)$(CXX)
FINAL_AR=$(CROSS_COMPILE)$(AR)
CPPFLAGS = -O3 -Wall -Wextra

PKG_CONFIG=pkg-config
INSTALL=install
LN=ln

DESTDIR ?=
PREFIX ?= /usr
INCDIR = $(DESTDIR)$(PREFIX)/include
LIBDIR = $(DESTDIR)$(PREFIX)/lib
BINDIR = $(DESTDIR)$(PREFIX)/bin
PKGDIR = $(LIBDIR)/pkgconfig

ifneq ($(DEBUG),)
    CPPFLAGS += -g
endif

# If the version is not set at make, read it from switchres.h
ifeq ($(VERSION),)
	VERSION:=$(shell grep -E "^\#define SWITCHRES_VERSION" switchres.h | grep -oE "[0-9]+\.[0-9]+\.[0-9]+" )
else
    CPPFLAGS += -DSWITCHRES_VERSION="\"$(VERSION)\""
endif
VERSION_MAJOR := $(firstword $(subst ., ,$(VERSION)))
VERSION_MINOR := $(word 2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(word 3,$(subst ., ,$(VERSION)))

$(info Switchres $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH))

# Linux
ifeq  ($(PLATFORM),Linux)
SRC += display_linux.cpp

HAS_VALID_XRANDR := $(shell $(PKG_CONFIG) --silence-errors --libs xrandr; echo $$?)
ifeq ($(HAS_VALID_XRANDR),1)
    $(info Switchres needs xrandr. X support is disabled)
else
    $(info X support enabled)
    CPPFLAGS += -DSR_WITH_XRANDR
    SRC += custom_video_xrandr.cpp
endif

HAS_VALID_DRMKMS := $(shell $(PKG_CONFIG) --silence-errors --libs "libdrm >= 2.4.98"; echo $$?)
ifeq ($(HAS_VALID_DRMKMS),1)
    $(info Switchres needs libdrm >= 2.4.98. KMS support is disabled)
else
    $(info KMS support enabled)
    CPPFLAGS += -DSR_WITH_KMSDRM
    EXTRA_LIBS = libdrm
    SRC += custom_video_drmkms.cpp
    ifeq ($(SR_WITH_DRMHOOK),1)
        CPPFLAGS += -DSR_WITH_DRMHOOK
    endif
endif

# SDL2 misses a test for drm as drm.h is required
HAS_VALID_SDL2 := $(shell $(PKG_CONFIG) --silence-errors --libs "sdl2 >= 2.0.16"; echo $$?)
ifeq ($(HAS_VALID_SDL2),1)
    $(info Switchres needs SDL2 >= 2.0.16. SDL2 support is disabled)
else
    $(info SDL2 support enabled)
    CPPFLAGS += -DSR_WITH_SDL2 $(pkg-config --cflags sdl2)
    EXTRA_LIBS += sdl2
    SRC += display_sdl2.cpp
endif

ifneq (,$(EXTRA_LIBS))
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags $(EXTRA_LIBS))
LIBS += $(shell $(PKG_CONFIG) --libs $(EXTRA_LIBS))
endif

CPPFLAGS += -fPIC
LIBS += -ldl

REMOVE = rm -f
STATIC_LIB_EXT = a
DYNAMIC_LIB_EXT = so.$(VERSION)
LINKER_NAME := $(TARGET_LIB).so
REAL_SO_NAME := $(LINKER_NAME).$(VERSION)
SO_NAME := $(LINKER_NAME).$(VERSION_MAJOR)
LIB_CPPFLAGS := -Wl,-soname,$(SO_NAME)
# Windows
else ifneq (,$(findstring NT,$(PLATFORM)))
SRC += display_windows.cpp custom_video_ati_family.cpp custom_video_ati.cpp custom_video_adl.cpp custom_video_pstrip.cpp resync_windows.cpp
WIN_ONLY_FLAGS = -static-libgcc -static-libstdc++
CPPFLAGS += -static $(WIN_ONLY_FLAGS)
LIBS =
#REMOVE = del /f
REMOVE = rm -f
STATIC_LIB_EXT = lib
DYNAMIC_LIB_EXT = dll
endif

define SR_PKG_CONFIG
prefix=$(PREFIX)
exec_prefix=$${prefix}
includedir=$${prefix}/include
libdir=$${exec_prefix}/lib

Name: libswitchres
Description: A modeline generator for CRT monitors
Version: $(VERSION)
Cflags: -I$${includedir}/switchres
Libs: -L$${libdir} -ldl -lswitchres
endef


%.o : %.cpp
	$(FINAL_CXX) -c $(CPPFLAGS) $< -o $@

all: $(SRC:.cpp=.o) $(MAIN).cpp $(TARGET_LIB) prepare_pkg_config
	@echo $(OSFLAG)
	$(FINAL_CXX) $(CPPFLAGS) $(CXXFLAGS) $(SRC:.cpp=.o) $(MAIN).cpp $(LIBS) -o $(STANDALONE)

$(TARGET_LIB): $(OBJS)
	$(FINAL_CXX) $(LDFLAGS) $(CPPFLAGS) $(LIB_CPPFLAGS) -o $@.$(DYNAMIC_LIB_EXT) $^
	$(FINAL_CXX) -c $(CPPFLAGS) -DSR_WIN32_STATIC switchres_wrapper.cpp -o switchres_wrapper.o
	$(FINAL_AR) rcs $@.$(STATIC_LIB_EXT) $(^)

$(DRMHOOK_LIB):
	$(FINAL_CXX) drm_hook.cpp -shared -ldl -fPIC -I/usr/include/libdrm  -o libdrmhook.so

$(GRID):
	$(FINAL_CXX) grid.cpp $(WIN_ONLY_FLAGS) -lSDL2 -lSDL2_ttf -o grid

clean:
	$(REMOVE) $(OBJS) $(STANDALONE) $(TARGET_LIB).*
	$(REMOVE) switchres.pc

prepare_pkg_config:
	$(file > switchres.pc,$(SR_PKG_CONFIG))

install:
	$(INSTALL) -Dm644 $(TARGET_LIB).$(DYNAMIC_LIB_EXT) $(LIBDIR)/$(TARGET_LIB).$(DYNAMIC_LIB_EXT)
	$(INSTALL) -Dm644 switchres_defines.h $(INCDIR)/switchres/switchres_defines.h
	$(INSTALL) -Dm644 switchres_wrapper.h $(INCDIR)/switchres/switchres_wrapper.h
	$(INSTALL) -Dm644 switchres.h $(INCDIR)/switchres/switchres.h
	$(INSTALL) -Dm644 switchres.pc $(PKGDIR)/switchres.pc
ifneq ($(SO_NAME),)
	$(LN) -s -f $(REAL_SO_NAME) $(LIBDIR)/$(SO_NAME)
	$(LN) -s -f $(SO_NAME) $(LIBDIR)/$(LINKER_NAME)
endif

uninstall:
	$(REMOVE) $(LIBDIR)/$(TARGET_LIB).*
	$(REMOVE) $(PKGDIR)/switchres.pc
