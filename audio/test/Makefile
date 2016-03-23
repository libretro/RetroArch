TESTS := test-sinc-lowest \
	test-snr-sinc-lowest \
	test-sinc-lower \
	test-snr-sinc-lower \
	test-sinc \
	test-snr-sinc \
	test-sinc-higher \
	test-snr-sinc-higher \
	test-sinc-highest \
	test-snr-sinc-highest \
	test-cc \
	test-snr-cc

LIBRETRO_COMM_DIR = ../../libretro-common

CFLAGS += -O3 -ffast-math -g -Wall -pedantic -march=native -std=gnu99
CFLAGS += -DRESAMPLER_TEST -DRARCH_DUMMY_LOG -DRARCH_INTERNAL
CFLAGS += -I$(LIBRETRO_COMM_DIR)/include -I../../

LDFLAGS += -lm

SHAREDOBJ += $(LIBRETRO_COMM_DIR)/memmap/memalign.o \
				 $(LIBRETRO_COMM_DIR)/lists/string_list.o \
				 ../..//config_file_userdata.o \
				 ../audio_resampler_driver.o \
				 $(LIBRETRO_COMM_DIR)/file/config_file.o \
				 $(LIBRETRO_COMM_DIR)/streams/file_stream.o \
				 $(LIBRETRO_COMM_DIR)/string/stdstring.o \
				 $(LIBRETRO_COMM_DIR)/file/file_path.o \
				 $(LIBRETRO_COMM_DIR)/file/retro_stat.o \
				 $(LIBRETRO_COMM_DIR)/compat/compat_strl.o \
				 $(LIBRETRO_COMM_DIR)/hash/rhash.o \

all: $(TESTS)

main-cc.o: main.c
	$(CC) -c -o $@ $< $(CFLAGS) -DRESAMPLER_IDENT='"CC"'

snr-cc.o: snr.c
	$(CC) -c -o $@ $< $(CFLAGS) -DRESAMPLER_IDENT='"CC"'

cc-resampler.o: ../drivers_resampler/cc_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS)

sinc-lowest.o: ../drivers_resampler/sinc_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_LOWEST_QUALITY

sinc-lower.o: ../drivers_resampler/sinc_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_LOWER_QUALITY

sinc.o: ../drivers_resampler/sinc_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS)

nearest_resampler.o: ../drivers_resampler/nearest_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS)

sinc-higher.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_HIGHER_QUALITY

sinc-highest.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_HIGHEST_QUALITY

test-sinc-lowest: sinc-lowest.o ../audio_utils.o main.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-lowest: sinc-lowest.o ../audio_utils.o snr.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-lower: sinc-lower.o ../audio_utils.o main.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-lower: sinc-lower.o ../audio_utils.o snr.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc: sinc.o ../audio_utils.o main.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc: sinc.o ../audio_utils.o snr.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-higher: sinc-higher.o ../audio_utils.o main.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-higher: sinc-higher.o ../audio_utils.o snr.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-highest: sinc-highest.o ../audio_utils.o main.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-highest: sinc-highest.o ../audio_utils.o snr.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-cc: cc-resampler.o ../audio_utils.o main-cc.o resampler-cc.o sinc.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-cc: cc-resampler.o ../audio_utils.o snr-cc.o resampler-cc.o sinc.o nearest_resampler.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -f ../*.o

.PHONY: clean

