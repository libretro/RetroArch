TARGET := rdvd_packet_test$(EXE)
TARGET_TEST := rdvd_highlight_test$(EXE)
CC ?= cc
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -I../../../include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES := rdvd_packet_test.c ../../../formats/dvd/rdvd_packet.c \
           ../../../formats/mpeg1/rmpeg1_ps.c

all: $(TARGET) $(TARGET_TEST)

$(TARGET): $(SOURCES) ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(TARGET_TEST): rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) -o $@ $(LDFLAGS)

check: $(TARGET) $(TARGET_TEST)
	./$(TARGET)
	./$(TARGET_TEST)

clean:
	rm -f rdvd_packet_test rdvd_packet_test.exe
	rm -f rdvd_highlight_test rdvd_highlight_test.exe

.PHONY: all check clean
