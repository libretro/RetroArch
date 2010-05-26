TARGET = ssnes

SOURCE = ssnes.c
CFLAGS = -Wall -O3 -march=native

OBJ = ssnes.o
SOBJ = libsnes.so

LIBS = -lrsound -lglfw -lsamplerate

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(SOBJ) $(LIBS)

ssnes.o:
	$(CC) -c -o ssnes.o ssnes.c $(CFLAGS)


clean:
	rm -rf $(OBJ)
	rm -rf $(TARGET)
