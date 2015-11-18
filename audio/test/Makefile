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
				 $(LIBRETRO_COMM_DIR)/string/string_list.o \
				 $(LIBRETRO_COMM_DIR)/file/config_file_userdata.o \
				 $(LIBRETRO_COMM_DIR)/file/config_file.o \
				 $(LIBRETRO_COMM_DIR)/file/retro_file.o \
				 $(LIBRETRO_COMM_DIR)/file/file_path.o \
				 $(LIBRETRO_COMM_DIR)/compat/compat.o \
				 $(LIBRETRO_COMM_DIR)/hash/rhash.o \

all: $(TESTS)

resampler-sinc.o: ../audio_resampler_driver.c $(SHAREDOBJ)
	$(CC) -c -o $@ $< $(CFLAGS)

resampler-cc.o: ../audio_resampler_driver.c $(SHAREDOBJ)
	$(CC) -c -o $@ $< $(CFLAGS) -DRESAMPLER_IDENT='"CC"'

main-cc.o: main.c
	$(CC) -c -o $@ $< $(CFLAGS) -DRESAMPLER_IDENT='"CC"'

snr-cc.o: snr.c
	$(CC) -c -o $@ $< $(CFLAGS) -DRESAMPLER_IDENT='"CC"'

cc-resampler.o: ../drivers_resampler/cc_resampler.c
	$(CC) -c -o $@ $< $(CFLAGS)

sinc-lowest.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_LOWEST_QUALITY

sinc-lower.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_LOWER_QUALITY

sinc.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS)

nearest.o: ../drivers_resampler/nearest.c
	$(CC) -c -o $@ $< $(CFLAGS)

sinc-higher.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_HIGHER_QUALITY

sinc-highest.o: ../drivers_resampler/sinc.c
	$(CC) -c -o $@ $< $(CFLAGS) -DSINC_HIGHEST_QUALITY

test-sinc-lowest: sinc-lowest.o ../audio_utils.o main.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-lowest: sinc-lowest.o ../audio_utils.o snr.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-lower: sinc-lower.o ../audio_utils.o main.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-lower: sinc-lower.o ../audio_utils.o snr.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc: sinc.o ../audio_utils.o main.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc: sinc.o ../audio_utils.o snr.o resampler-sinc.o resampler-cc.o  nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-higher: sinc-higher.o ../audio_utils.o main.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-higher: sinc-higher.o ../audio_utils.o snr.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-sinc-highest: sinc-highest.o ../audio_utils.o main.o resampler-sinc.o resampler-cc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-sinc-highest: sinc-highest.o ../audio_utils.o snr.o resampler-sinc.o resampler-cc.o  nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-cc: cc-resampler.o ../audio_utils.o main-cc.o resampler-cc.o resampler-cc.o sinc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

test-snr-cc: cc-resampler.o ../audio_utils.o snr-cc.o resampler-cc.o sinc.o nearest.o $(SHAREDOBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TESTS)
	rm -f *.o
	rm -f ../*.o

.PHONY: clean

