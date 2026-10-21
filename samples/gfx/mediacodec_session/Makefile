CC ?= cc
JAVA_HOME ?= $(shell dirname $$(dirname $$(readlink -f $$(command -v javac))))
CFLAGS += -DHAVE_DYLIB -DMEDIACODEC_SESSION_TEST -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I. -I../../../libretro-common/include -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: test hevc_decoder_test vulkan_image_test vulkan_sync_test vulkan_pipeline_test
test: test.c mediacodec_session_test_types.h ../../../gfx/common/mediacodec_session.c
	$(CC) $(CFLAGS) test.c ../../../gfx/common/mediacodec_session.c -o $@ $(LDFLAGS)
HEVC_SOURCES := ../../../gfx/common/mediacodec_hevc_decoder.c \
 ../../../gfx/common/mediacodec_hevc_packet.c ../../../gfx/common/android_image_queue.c
hevc_decoder_test: hevc_decoder_test.c $(HEVC_SOURCES) \
 ../../../gfx/common/mediacodec_hevc_decoder.h ../../../gfx/common/android_image_queue.h \
 ../../../libretro-common/include/formats/rh265_hw.h
	$(CC) $(CFLAGS) -DHAVE_THREADS hevc_decoder_test.c $(HEVC_SOURCES) -o $@ $(LDFLAGS)
vulkan_image_test: vulkan_image_test.c ../../../gfx/common/vulkan_android_image.c ../../../gfx/common/vulkan_android_image.h
	$(CC) $(CFLAGS) -I../../../gfx/include vulkan_image_test.c ../../../gfx/common/vulkan_android_image.c -o $@ $(LDFLAGS)
vulkan_sync_test: vulkan_sync_test.c ../../../gfx/common/vulkan_android_sync.c ../../../gfx/common/vulkan_android_sync.h
	$(CC) $(CFLAGS) -I../../../gfx/include vulkan_sync_test.c ../../../gfx/common/vulkan_android_sync.c -o $@ $(LDFLAGS)
vulkan_pipeline_test: vulkan_pipeline_test.c ../../../gfx/common/vulkan_android_pipeline.c ../../../gfx/common/vulkan_android_pipeline.h
	$(CC) $(CFLAGS) -I../../../gfx/include vulkan_pipeline_test.c ../../../gfx/common/vulkan_android_pipeline.c -o $@ $(LDFLAGS)
check: all
	./test
	./hevc_decoder_test
	./vulkan_image_test
	./vulkan_sync_test
	./vulkan_pipeline_test
clean:
	rm -f test hevc_decoder_test vulkan_image_test vulkan_sync_test vulkan_pipeline_test
.PHONY: all check clean
