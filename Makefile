include config.mk

TARGET = ssnes

OBJ = ssnes.o file.o driver.o conf/config_file.o settings.o dynamic.o

LIBS = -lsamplerate
DEFINES =

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

ifeq ($(HAVE_GLFW), 1)
   OBJ += gfx/gl.o
   LIBS += -lglfw
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

CFLAGS = -Wall -O3 -g -std=gnu99 -I.

all: $(TARGET) config.mk

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

ssnes: $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LIBS) $(CFLAGS)

%.o: %.c config.h config.mk
	$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

install: $(TARGET)
	install -m755 $(TARGET) $(DESTDIR)/$(PREFIX)/bin 
	install -m644 ssnes.cfg $(DESTDIR)/etc/ssnes.cfg

uninstall: $(TARGET)
	rm -rf $(DESTDIR)/$(PREFIX)/bin/$(TARGET)

clean:
	rm -f *.o 
	rm -f audio/*.o
	rm -f conf/*.o
	rm -f gfx/*.o
	rm -f hqflt/*.o
	rm -f hqflt/snes_ntsc/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
