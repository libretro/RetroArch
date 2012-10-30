TESTS := test-hermite test-sinc test-snr-sinc test-snr-hermite

CFLAGS += -O3 -g -Wall -pedantic -std=gnu99 -DRESAMPLER_TEST
LDFLAGS += -lm

all: $(TESTS)

test-hermite: ../hermite.o ../utils.o main.o
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc: ../sinc.o ../utils.o main.o
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc: ../sinc.o ../utils.o snr.o
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-hermite: ../hermite.o ../utils.o snr.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -f ../*.o

.PHONY: clean

