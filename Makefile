include config.mk

TARGET = ssnes tools/ssnes-joyconfig

OBJ = ssnes.o file.o driver.o settings.o dynamic.o message.o rewind.o movie.o autosave.o gfx/gfx_common.o ups.o strl.o screenshot.o
JOYCONFIG_OBJ = tools/ssnes-joyconfig.o conf/config_file.o
HEADERS = $(wildcard */*.h) $(wildcard *.h)

LIBS = -lm
DEFINES = -DHAVE_CONFIG_H

ifneq ($(findstring Darwin,$(shell uname -a)),)
   OSX := 1
   LIBS += -framework AppKit
else
   OSX := 0
endif

BSD_LOCAL_INC =
DYLIB_LIB = -ldl
ifneq ($(findstring BSD,$(shell uname -a)),)
   BSD_LOCAL_INC = -I/usr/local/include
   DYLIB_LIB = -lc
endif

ifeq ($(HAVE_SRC), 1)
   LIBS += $(SRC_LIBS)
   DEFINES += $(SRC_CFLAGS)
else
   OBJ += audio/hermite.o
endif

ifeq ($(HAVE_CONFIGFILE), 1)
   OBJ += conf/config_file.o
endif

ifeq ($(HAVE_NETPLAY), 1)
   OBJ += netplay.o
endif

ifeq ($(HAVE_RSOUND), 1)
   OBJ += audio/rsound.o
   LIBS += -lrsound
endif

ifeq ($(HAVE_OSS), 1)
   OBJ += audio/oss.o
endif
ifeq ($(HAVE_ALSA), 1)
   OBJ += audio/alsa.o
   LIBS += -lasound
endif
ifeq ($(HAVE_ROAR), 1)
   OBJ += audio/roar.o
   LIBS += -lroar
endif
ifeq ($(HAVE_AL), 1)
   OBJ += audio/openal.o
ifeq ($(OSX),1)
   LIBS += -framework OpenAL
else
   LIBS += -lopenal
endif
endif
ifeq ($(HAVE_JACK),1)
   OBJ += audio/jack.o
   LIBS += $(JACK_LIBS)
   DEFINES += $(JACK_CFLAGS)
endif
ifeq ($(HAVE_PULSE), 1)
   OBJ += audio/pulse.o
   LIBS += $(PULSE_LIBS)
   DEFINES += $(PULSE_CFLAGS)
endif

ifeq ($(HAVE_SDL), 1)
   OBJ += gfx/sdl.o gfx/gl.o input/sdl.o audio/sdl.o fifo_buffer.o
   DEFINES += $(SDL_CFLAGS) $(BSD_LOCAL_INC)
   LIBS += $(SDL_LIBS)
ifeq ($(OSX),1)
   LIBS += -framework OpenGL
else
   LIBS += -lGL
endif
endif

ifeq ($(HAVE_XVIDEO), 1)
   OBJ += gfx/xvideo.o input/x11_input.o
   LIBS += -lX11 -lXext -lXv
endif

ifeq ($(HAVE_CG), 1)
   OBJ += gfx/shader_cg.o
   LIBS += -lCg -lCgGL
endif

ifeq ($(HAVE_XML), 1)
   OBJ += gfx/shader_glsl.o gfx/image.o gfx/snes_state.o sha256.o cheats.o 
   LIBS += $(XML_LIBS)
   DEFINES += $(XML_CFLAGS)
endif

ifeq ($(HAVE_DYLIB), 1)
   OBJ += gfx/ext.o audio/ext.o
   LIBS += $(DYLIB_LIB)
endif

ifeq ($(HAVE_FREETYPE), 1)
   OBJ += gfx/fonts.o
   LIBS += $(FREETYPE_LIBS)
   DEFINES += $(FREETYPE_CFLAGS)
endif

ifeq ($(HAVE_SDL_IMAGE), 1)
   LIBS += $(SDL_IMAGE_LIBS)
   DEFINES += $(SDL_IMAGE_CFLAGS)
endif

ifeq ($(HAVE_FFMPEG), 1)
   OBJ += record/ffemu.o
   LIBS += $(AVCODEC_LIBS) $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS)
   DEFINES += $(AVCODEC_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
endif

ifeq ($(HAVE_DYNAMIC), 1)
   LIBS += $(DYLIB_LIB)
else
   LIBS += $(libsnes)
endif

ifeq ($(HAVE_STRL), 1)
   DEFINES += -DHAVE_STRL
endif

ifeq ($(HAVE_PYTHON), 1)
   DEFINES += $(PYTHON_CFLAGS) -Wno-unused-parameter
   LIBS += $(PYTHON_LIBS)
   OBJ += gfx/py_state/py_state.o
endif

ifneq ($(V),1)
   Q := @
endif

CFLAGS += -Wall -O3 -g -I. -pedantic
ifneq ($(findstring icc,$(CC)),)
   CFLAGS += -std=c99
else
   CFLAGS += -std=gnu99
endif

all: $(TARGET) config.mk

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

ssnes: $(OBJ)
	$(Q)$(CXX) -o $@ $(OBJ) $(LIBS) $(LDFLAGS)
	@$(if $(Q), $(shell echo echo LD $@),)

tools/ssnes-joyconfig: $(JOYCONFIG_OBJ)
	$(Q)$(CC) -o $@ $(JOYCONFIG_OBJ) $(SDL_LIBS) $(LDFLAGS)
	@$(if $(Q), $(shell echo echo LD $@),)

%.o: %.c config.h config.mk $(HEADERS)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<
	@$(if $(Q), $(shell echo echo CC $<),)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)/etc 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m644 ssnes.cfg $(DESTDIR)/etc/ssnes.cfg
	install -m644 docs/ssnes.1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m644 docs/ssnes-joyconfig.1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m755 ssnes-zip $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/ssnes
	rm -f $(DESTDIR)$(PREFIX)/bin/ssnes-joyconfig
	rm -f $(DESTDIR)$(PREFIX)/bin/ssnes-zip
	rm -f $(DESTDIR)/etc/ssnes.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/ssnes.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/ssnes-joyconfig.1

clean:
	rm -f *.o 
	rm -f audio/*.o
	rm -f conf/*.o
	rm -f gfx/*.o
	rm -f gfx/py_state/*.o
	rm -f record/*.o
	rm -f input/*.o
	rm -f tools/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
