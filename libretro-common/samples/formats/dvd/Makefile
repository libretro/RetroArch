TARGET := rdvd_packet_test$(EXE)
CC ?= cc
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -I../../../include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES := rdvd_packet_test.c ../../../formats/dvd/rdvd_packet.c

all: $(TARGET)

$(TARGET): $(SOURCES) ../../../include/formats/rdvd_packet.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f rdvd_packet_test rdvd_packet_test.exe

.PHONY: all check clean
