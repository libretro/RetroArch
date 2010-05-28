TARGET = ssnes

SOURCE = ssnes.c rsound.c gl.c
CFLAGS = -Wall -O3 -march=native

OBJ = ssnes.o
SOBJ = libsnes.so

LIBS = -lrsound -lglfw -lsamplerate

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(SOBJ) $(LIBS)

clean:
	rm -rf $(OBJ)
	rm -rf $(TARGET)
