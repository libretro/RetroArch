include config.mk

TARGET = ssnes

OBJ = ssnes.o
LIBS = -lsamplerate libsnes.a

ifeq ($(BUILD_RSOUND), 1)
   OBJ += rsound.o
   LIBS += -lrsound
endif
ifeq ($(BUILD_OSS), 1)
   OBJ += oss.o
endif
ifeq ($(BUILD_ALSA), 1)
   OBJ += alsa.o
   LIBS += -lasound
endif
ifeq ($(BUILD_ROAR), 1)
   OBJ += roar.o
   LIBS += -lroar
endif
ifeq ($(BUILD_AL), 1)
   OBJ += openal.o
   LIBS += -lopenal
endif

ifeq ($(BUILD_OPENGL), 1)
   OBJ += gl.o
   LIBS += -lglfw
endif
ifeq ($(BUILD_FILTER), 1)
   OBJ += hqflt/hq.o
   OBJ += hqflt/grayscale.o
   OBJ += hqflt/bleed.o
   OBJ += hqflt/ntsc.o
   OBJ += hqflt/snes_ntsc/snes_ntsc.o
endif

CFLAGS = -Wall -O3 -march=native -std=gnu99

all: $(TARGET) 

ssnes: $(OBJ)
	@$(CXX) -o $@ $(OBJ) $(LIBS)
	@echo "LD $@"

%.o: %.c config.h config.mk
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo "CC $<"

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
