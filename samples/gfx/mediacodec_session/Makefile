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
all: test hevc_decoder_test vulkan_image_test vulkan_sync_test vulkan_pipeline_test android_frame_test frame_export_test vulkan_submit_test vulkan_entry_test
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
vulkan_rgb_gpu_test: vulkan_rgb_gpu_test.c ../../../gfx/common/vulkan_android_pipeline.c ../../../gfx/common/vulkan_android_pipeline.h \
 ../../../gfx/drivers/vulkan_shaders/external_rgb_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/external_rgb_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) -I../../../gfx/include vulkan_rgb_gpu_test.c ../../../gfx/common/vulkan_android_pipeline.c -o $@ $(LDFLAGS) -lvulkan -lm
gpu-check: vulkan_rgb_gpu_test
	./vulkan_rgb_gpu_test
android_frame_test: android_frame_test.c ../../../gfx/common/android_frame.c ../../../gfx/common/android_frame.h ../../../gfx/common/android_image_queue.c
	$(CC) $(CFLAGS) -DHAVE_THREADS -pthread android_frame_test.c ../../../gfx/common/android_frame.c ../../../gfx/common/android_image_queue.c -o $@ $(LDFLAGS)
EXPORT_SOURCES := ../../../gfx/common/mediacodec_frame_export.c ../../../gfx/common/mediacodec_color.c \
 ../../../gfx/common/android_frame.c ../../../gfx/common/android_image_queue.c
frame_export_test: frame_export_test.c $(EXPORT_SOURCES) ../../../gfx/common/mediacodec_frame_export.h ../../../gfx/common/mediacodec_color.h
	$(CC) $(CFLAGS) -DHAVE_THREADS frame_export_test.c $(EXPORT_SOURCES) -o $@ $(LDFLAGS)
SUBMIT_SOURCES := ../../../gfx/common/vulkan_android_submit.c ../../../gfx/common/vulkan_android_image.c \
 ../../../gfx/common/vulkan_android_sync.c ../../../gfx/common/vulkan_android_pipeline.c \
 ../../../gfx/common/android_frame.c ../../../gfx/common/android_image_queue.c \
 ../../../libretro-common/formats/image/image_yuv_blit.c
vulkan_submit_test: vulkan_submit_test.c $(SUBMIT_SOURCES) ../../../gfx/common/vulkan_android_submit.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -I../../../gfx/include vulkan_submit_test.c $(SUBMIT_SOURCES) -o $@ $(LDFLAGS) -lm
vulkan_entry_test: vulkan_entry_test.c vulkan_submit_test.c $(SUBMIT_SOURCES) ../../../gfx/common/vulkan_android_cache_entry.c ../../../gfx/common/vulkan_android_cache_entry.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -I../../../gfx/include vulkan_entry_test.c $(SUBMIT_SOURCES) ../../../gfx/common/vulkan_android_cache_entry.c -o $@ $(LDFLAGS) -lm
check: all
	./test
	./hevc_decoder_test
	./vulkan_image_test
	./vulkan_sync_test
	./vulkan_pipeline_test
	./android_frame_test
	./frame_export_test
	./vulkan_submit_test
	./vulkan_entry_test
clean:
	rm -f test hevc_decoder_test vulkan_image_test vulkan_sync_test vulkan_pipeline_test vulkan_rgb_gpu_test android_frame_test frame_export_test vulkan_submit_test vulkan_entry_test
.PHONY: all check clean gpu-check
