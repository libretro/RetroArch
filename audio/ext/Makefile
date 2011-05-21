TARGET = ssnes_iir_filter.so
OBJ = ssnes_iir_filter.o

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ -shared $(OBJ) -lm

%.o: %.c
	$(CC) -c -o $@ $< -g -std=c99 -fPIC -O3 -march=native $(CFLAGS)

clean:
	rm -f $(TARGET)
	rm -f $(OBJ)

.PHONY: clean
