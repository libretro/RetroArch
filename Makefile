include config.mk

TARGET = ssnes

DEFINES =
OBJ = ssnes.o file.o driver.o
libsnes = -lsnes

LIBS = -lsamplerate $(libsnes)

ifeq ($(BUILD_RSOUND), 1)
   OBJ += audio/rsound.o
   LIBS += -lrsound
endif
ifeq ($(BUILD_OSS), 1)
   OBJ += audio/oss.o
endif
ifeq ($(BUILD_ALSA), 1)
   OBJ += audio/alsa.o
   LIBS += -lasound
endif
ifeq ($(BUILD_ROAR), 1)
   OBJ += audio/roar.o
   LIBS += -lroar
endif
ifeq ($(BUILD_AL), 1)
   OBJ += audio/openal.o
   LIBS += -lopenal
endif

ifeq ($(BUILD_OPENGL), 1)
   OBJ += gfx/gl.o
   LIBS += -lglfw
endif

ifeq ($(BUILD_CG), 1)
   LIBS += -lCg -lCgGL
   DEFINES += -DHAVE_CG 
endif

ifeq ($(BUILD_FILTER), 1)
   OBJ += hqflt/hq.o
   OBJ += hqflt/grayscale.o
   OBJ += hqflt/bleed.o
   OBJ += hqflt/ntsc.o
   OBJ += hqflt/snes_ntsc/snes_ntsc.o
endif

CFLAGS = -Wall -O3 -std=gnu99 -Wno-unused-variable -I. $(DEFINES)

all: $(TARGET) 

ssnes: $(OBJ)
	$(CXX) -o $@ $(OBJ) $(LIBS) $(CFLAGS)

%.o: %.c config.h config.mk
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -m755 $(TARGET) $(PREFIX)/bin 

uninstall: $(TARGET)
	rm -rf $(PREFIX)/bin/$(TARGET)

clean:
	rm -f *.o 
	rm -f hqflt/*.o
	rm -f hqflt/snes_ntsc/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
