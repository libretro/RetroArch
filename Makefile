include config.mk

TARGET = ssnes

OBJ = ssnes.o
LIBS = -lsamplerate -lsnes

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

ifeq ($(BUILD_OPENGL), 1)
   OBJ += gl.o
   LIBS += -lglfw
endif
ifeq ($(BUILD_FILTER), 1)
   OBJ += hqflt/hq.o
endif

CFLAGS = -Wall -O3 -march=native -std=c99



all: $(TARGET) 

ssnes: $(OBJ)
	@$(CC) -o $@ $(OBJ) $(LIBS)
	@echo "LD $@"

%.o: %.c config.h config.mk
	@$(CC) $(CFLAGS) -c -o $@ $<
	@echo "CC $<"

install: $(TARGET)
	install -m755 $(TARGET) $(PREFIX)/bin 

uninstall: $(TARGET)
	rm -rf $(PREFIX)/bin/$(TARGET)

clean:
	rm -rf *.o hqflt/*.o
	rm -rf $(TARGET)

.PHONY: all install uninstall clean
