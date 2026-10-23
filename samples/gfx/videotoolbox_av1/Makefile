CC ?= cc
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: config_test$(EXE) packet_test$(EXE) reference_test$(EXE)
config_test$(EXE): config_test.c ../../../gfx/common/videotoolbox_av1_config.h ../vulkan_video_caps/av1_session_data.h $(wildcard ../../../libretro-common/include/formats/rav1*.h)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
packet_test$(EXE): packet_test.c ../../../gfx/common/videotoolbox_av1_packet.h ../../../libretro-common/include/formats/rav1.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
reference_test$(EXE): reference_test.c native_reference.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lm
check: all
	./config_test$(EXE)
	./packet_test$(EXE)
	./reference_test$(EXE)
	CC="$(CC)" CFLAGS="$(CFLAGS) $(LDFLAGS)" python3 run.py
clean:
	rm -f config_test config_test.exe packet_test packet_test.exe reference_test reference_test.exe
native-check:
	CC="$(CC)" python3 run.py --native-pixels --pipeline
.PHONY: all check clean native-check
