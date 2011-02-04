include config.mk

TARGET = ssnes tools/ssnes-joyconfig

OBJ = ssnes.o file.o driver.o settings.o dynamic.o message.o rewind.o movie.o
JOYCONFIG_OBJ = tools/ssnes-joyconfig.o conf/config_file.o
HEADERS = $(wildcard */*.h) $(wildcard *.h)

LIBS =
DEFINES = -DHAVE_CONFIG_H

ifeq ($(HAVE_SRC), 1)
   LIBS += $(SRC_LIBS)
   DEFINES += $(SRC_CFLAGS)
endif

ifeq ($(HAVE_CONFIGFILE), 1)
   OBJ += conf/config_file.o
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
   LIBS += -lopenal
endif
ifeq ($(HAVE_JACK),1)
   OBJ += audio/jack.o
   LIBS += -ljack
endif
ifeq ($(HAVE_PULSE), 1)
   OBJ += audio/pulse.o
   LIBS += $(PULSE_LIBS)
   DEFINES += $(PULSE_CFLAGS)
endif

ifeq ($(HAVE_SDL), 1)
   OBJ += gfx/gl.o input/sdl.o audio/sdl.o audio/buffer.o
   DEFINES += $(SDL_CFLAGS)
   LIBS += $(SDL_LIBS)
ifneq ($(findstring Darwin,$(shell uname -a)),)
   LIBS += -framework OpenGL
else
   LIBS += -lGL
endif
endif

ifeq ($(HAVE_CG), 1)
   OBJ += gfx/shader_cg.o
   LIBS += -lCg -lCgGL
endif

ifeq ($(HAVE_XML), 1)
   OBJ += gfx/shader_glsl.o
   LIBS += $(XML_LIBS)
   DEFINES += $(XML_CFLAGS)
endif

ifeq ($(HAVE_FILTER), 1)
   OBJ += hqflt/hq.o
   OBJ += hqflt/grayscale.o
   OBJ += hqflt/bleed.o
   OBJ += hqflt/ntsc.o
   OBJ += hqflt/snes_ntsc/snes_ntsc.o
endif

ifeq ($(HAVE_FREETYPE), 1)
   OBJ += gfx/fonts.o
   LIBS += $(FREETYPE_LIBS)
   DEFINES += $(FREETYPE_CFLAGS)
endif

ifeq ($(HAVE_FFMPEG), 1)
   OBJ += record/ffemu.o
   LIBS += $(AVCODEC_LIBS) $(AVCORE_LIBS) $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS)
   DEFINES += $(AVCODEC_CFLAGS) $(AVCORE_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
endif

ifeq ($(HAVE_DYNAMIC), 1)
   LIBS += -ldl
else
   LIBS += $(libsnes)
endif

ifneq ($(V),1)
   Q := @
endif

CFLAGS = -Wall -O3 -g -std=gnu99 -I.

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
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)/etc
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m644 ssnes.cfg $(DESTDIR)/etc/ssnes.cfg
	install -m644 docs/{ssnes,ssnes-joyconfig}.1 $(DESTDIR)$(PREFIX)/share/man/man1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/{ssnes,ssnes-joyconfig}
	rm -f $(DESTDIR)/etc/ssnes.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/{ssnes,ssnes-joyconfig}.1

clean:
	rm -f *.o 
	rm -f audio/*.o
	rm -f conf/*.o
	rm -f gfx/*.o
	rm -f record/*.o
	rm -f hqflt/*.o
	rm -f hqflt/snes_ntsc/*.o
	rm -f input/*.o
	rm -f tools/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
