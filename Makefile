include config.mk

TARGET = ssnes

OBJ = ssnes.o file.o driver.o conf/config_file.o settings.o

LIBS = -lsamplerate $(libsnes)

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

ifeq ($(HAVE_GLFW), 1)
   OBJ += gfx/gl.o
   LIBS += -lglfw
endif

ifeq ($(HAVE_CG), 1)
   LIBS += -lCg -lCgGL
endif

ifeq ($(HAVE_FILTER), 1)
   OBJ += hqflt/hq.o
   OBJ += hqflt/grayscale.o
   OBJ += hqflt/bleed.o
   OBJ += hqflt/ntsc.o
   OBJ += hqflt/snes_ntsc/snes_ntsc.o
endif

CFLAGS = -Wall -O3 -std=gnu99 -I.

all: $(TARGET) config.mk

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

ssnes: $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LIBS) $(CFLAGS)

%.o: %.c config.h config.mk
	$(CC) $(CFLAGS) -c -o $@ $<

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
