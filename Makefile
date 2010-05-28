include config.mak

TARGET = ssnes

SOURCE = ssnes.c 
LIBS = -lsamplerate -lsnes

ifeq ($(BUILD_RSOUND), 1)
   SOURCE += rsound.c
   LIBS += -lrsound
endif
ifeq ($(BUILD_OSS), 1)
   SOURCE += oss.c
endif
ifeq ($(BUILD_ALSA), 1)
   SOURCE += alsa.c
   LIBS += -lasound
endif

ifeq ($(BUILD_OPENGL), 1)
   SOURCE += gl.c
   LIBS += -lglfw
endif

CFLAGS = -Wall -O2 -march=native -s

OBJ = ssnes.o



all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LIBS)

install: $(TARGET)
	install -m755 $(TARGET) $(PREFIX)/bin 

uninstall: $(TARGET)
	rm -rf $(PREFIX)/bin/$(TARGET)

clean:
	rm -rf $(OBJ)
	rm -rf $(TARGET)
