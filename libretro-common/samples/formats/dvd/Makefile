TARGET := rdvd_packet_test$(EXE)
TARGET_TEST := rdvd_highlight_test$(EXE)
TARGET_TEST2 := rdvd_spu_test$(EXE)
TARGET_TEST3 := rdvd_ifo_test$(EXE)
TARGET_TEST4 := rdvd_source_test$(EXE)
TARGET_TEST5 := rdvd_vfs_test$(EXE)
CC ?= cc
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -I../../../include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES := rdvd_packet_test.c ../../../formats/dvd/rdvd_packet.c \
           ../../../formats/mpeg1/rmpeg1_ps.c

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5)

$(TARGET): $(SOURCES) ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(TARGET_TEST): rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) -o $@ $(LDFLAGS)

$(TARGET_TEST2): rdvd_spu_test.c ../../../formats/dvd/rdvd_spu.c ../../../include/formats/rdvd_spu.h
	$(CC) $(CFLAGS) rdvd_spu_test.c ../../../formats/dvd/rdvd_spu.c -o $@ $(LDFLAGS)

$(TARGET_TEST3): rdvd_ifo_test.c ../../../formats/dvd/rdvd_ifo.c ../../../include/formats/rdvd_ifo.h
	$(CC) $(CFLAGS) rdvd_ifo_test.c ../../../formats/dvd/rdvd_ifo.c -o $@ $(LDFLAGS)

$(TARGET_TEST4): rdvd_source_test.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../include/formats/rdvd_source.h ../../../include/formats/rdvd_ifo.h
	$(CC) $(CFLAGS) rdvd_source_test.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c -o $@ $(LDFLAGS)

$(TARGET_TEST5): rdvd_vfs_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c ../../../include/formats/rdvd_vfs.h
	$(CC) $(CFLAGS) -DHAVE_THREADS rdvd_vfs_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c -o $@ $(LDFLAGS)

check: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5)
	./$(TARGET)
	./$(TARGET_TEST)
	./$(TARGET_TEST2)
	./$(TARGET_TEST3)
	./$(TARGET_TEST4)
	./$(TARGET_TEST5)

clean:
	rm -f rdvd_packet_test rdvd_packet_test.exe
	rm -f rdvd_highlight_test rdvd_highlight_test.exe
	rm -f rdvd_spu_test rdvd_spu_test.exe
	rm -f rdvd_ifo_test rdvd_ifo_test.exe

	rm -f rdvd_source_test rdvd_source_test.exe

	rm -f rdvd_vfs_test rdvd_vfs_test.exe

.PHONY: all check clean
