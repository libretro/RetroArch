TARGET := rdvd_packet_test$(EXE)
TARGET_TEST := rdvd_highlight_test$(EXE)
TARGET_TEST2 := rdvd_spu_test$(EXE)
TARGET_TEST3 := rdvd_ifo_test$(EXE)
TARGET_TEST4 := rdvd_source_test$(EXE)
TARGET_TEST5 := rdvd_vfs_test$(EXE)
TARGET_TEST6 := rdvd_vfs_real_test$(EXE)
TARGET_TEST7 := rdvd_ac3_test$(EXE)
TARGET_TEST8 := rdvd_lpcm_test$(EXE)
TARGET_TEST9 := rdvd_h262_test$(EXE)
CC ?= cc
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -I../../../include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES := rdvd_packet_test.c ../../../formats/dvd/rdvd_demux.c ../../../formats/dvd/rdvd_packet.c \
           ../../../formats/mpeg1/rmpeg1_ps.c

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5) $(TARGET_TEST6) $(TARGET_TEST7) $(TARGET_TEST8) $(TARGET_TEST9)

$(TARGET): $(SOURCES) ../../../include/formats/rdvd_demux.h ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

$(TARGET_TEST): rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) ../../../include/formats/rdvd_packet.h ../../../include/formats/rmpeg1_ps.h
	$(CC) $(CFLAGS) rdvd_highlight_test.c $(filter-out rdvd_packet_test.c,$(SOURCES)) -o $@ $(LDFLAGS)

$(TARGET_TEST2): rdvd_spu_test.c ../../../formats/dvd/rdvd_spu.c ../../../include/formats/rdvd_spu.h
	$(CC) $(CFLAGS) rdvd_spu_test.c ../../../formats/dvd/rdvd_spu.c -o $@ $(LDFLAGS)

$(TARGET_TEST3): rdvd_ifo_test.c ../../../formats/dvd/rdvd_ifo.c ../../../include/formats/rdvd_ifo.h
	$(CC) $(CFLAGS) rdvd_ifo_test.c ../../../formats/dvd/rdvd_ifo.c -o $@ $(LDFLAGS)

$(TARGET_TEST4): rdvd_source_test.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../include/formats/rdvd_source.h ../../../include/formats/rdvd_ifo.h
	$(CC) $(CFLAGS) rdvd_source_test.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c -o $@ $(LDFLAGS)

DVD_STREAM_HEADERS := ../../../include/formats/rdvd_stream.h ../../../include/formats/rdvd_demux.h
DVD_STREAM_SOURCES := ../../../formats/dvd/rdvd_stream.c ../../../formats/dvd/rdvd_demux.c ../../../formats/dvd/rdvd_packet.c ../../../formats/mpeg1/rmpeg1_ps.c

$(TARGET_TEST5): $(DVD_STREAM_HEADERS) $(DVD_STREAM_SOURCES) rdvd_vfs_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c ../../../include/formats/rdvd_vfs.h
	$(CC) $(CFLAGS) -DHAVE_THREADS $(DVD_STREAM_SOURCES) rdvd_vfs_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c -o $@ $(LDFLAGS)

ifneq ($(findstring mingw,$(shell $(CC) -dumpmachine)),)
VFS_REAL_LIBS :=
else
VFS_REAL_LIBS := -lpthread -lrt
endif
VFS_REAL_SOURCES := ../../../formats/dvd/rdvd_lpcm.c ../../../formats/lpcm/rlpcm.c ../../../formats/dvd/rdvd_ac3.c ../../../formats/ac3/rac3_frame.c ../../../formats/ac3/rac3_decode.c ../../../rthreads/tpool.c ../../../rthreads/retro_eventcount.c ../../../rthreads/rthreads.c ../../../features/features_cpu.c ../../../compat/fopen_utf8.c ../../../compat/compat_strl.c ../../../encodings/encoding_utf.c ../../../file/file_path.c ../../../file/file_path_io.c ../../../file/retro_dirent.c ../../../streams/file_stream.c ../../../string/rstrtod.c ../../../time/rtime.c ../../../vfs/vfs_implementation.c
$(TARGET_TEST6): $(DVD_STREAM_HEADERS) $(DVD_STREAM_SOURCES) rdvd_vfs_real_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c $(VFS_REAL_SOURCES)
	$(CC) $(filter-out -std=c89 -Werror,$(CFLAGS)) -std=gnu99 -DHAVE_THREADS $(DVD_STREAM_SOURCES) rdvd_vfs_real_test.c ../../../formats/dvd/rdvd_vfs.c ../../../formats/dvd/rdvd_source.c ../../../formats/dvd/rdvd_ifo.c ../../../queues/retro_spsc.c $(VFS_REAL_SOURCES) -o $@ $(LDFLAGS) $(VFS_REAL_LIBS) -lm

AC3_SOURCES := ../../../formats/dvd/rdvd_ac3.c ../../../formats/ac3/rac3_frame.c
$(TARGET_TEST7): rdvd_ac3_test.c $(AC3_SOURCES) ../../../include/formats/rdvd_ac3.h
	$(CC) $(CFLAGS) rdvd_ac3_test.c $(AC3_SOURCES) -o $@ $(LDFLAGS)

$(TARGET_TEST8): rdvd_lpcm_test.c ../../../formats/dvd/rdvd_lpcm.c ../../../formats/lpcm/rlpcm.c ../../../include/formats/rlpcm.h ../../../include/formats/rdvd_lpcm.h
	$(CC) $(CFLAGS) rdvd_lpcm_test.c ../../../formats/dvd/rdvd_lpcm.c ../../../formats/lpcm/rlpcm.c -o $@ $(LDFLAGS)

$(TARGET_TEST9): rdvd_h262_test.c ../../../formats/mpeg1/rmpeg1_video.c ../../../include/formats/rmpeg1_video.h
	$(CC) $(CFLAGS) rdvd_h262_test.c ../../../formats/mpeg1/rmpeg1_video.c -o $@ $(LDFLAGS)

check: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4) $(TARGET_TEST5) $(TARGET_TEST6) $(TARGET_TEST7) $(TARGET_TEST8) $(TARGET_TEST9)
	./$(TARGET)
	./$(TARGET_TEST)
	./$(TARGET_TEST2)
	./$(TARGET_TEST3)
	./$(TARGET_TEST4)
	./$(TARGET_TEST5)
	./$(TARGET_TEST6)
	./$(TARGET_TEST7)
	./$(TARGET_TEST8)
	./$(TARGET_TEST9)

clean:
	rm -f rdvd_packet_test rdvd_packet_test.exe
	rm -f rdvd_highlight_test rdvd_highlight_test.exe
	rm -f rdvd_spu_test rdvd_spu_test.exe
	rm -f rdvd_ifo_test rdvd_ifo_test.exe

	rm -f rdvd_source_test rdvd_source_test.exe

	rm -f rdvd_vfs_test rdvd_vfs_test.exe

	rm -f rdvd_vfs_real_test rdvd_vfs_real_test.exe

	rm -f rdvd_ac3_test rdvd_ac3_test.exe

	rm -f rdvd_lpcm_test rdvd_lpcm_test.exe

	rm -f rdvd_h262_test rdvd_h262_test.exe

.PHONY: all check clean
