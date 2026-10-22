CC ?= cc
VAAPI_CFLAGS ?= $(shell pkg-config --cflags libva libdrm)
CFLAGS += -DHAVE_DYLIB -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
 -Wframe-larger-than=2048 -I../../../libretro-common/include -I../../../gfx/include $(VAAPI_CFLAGS)
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
ifeq ($(shell uname -s),Linux)
NATIVE_PROBE = native_probe
FRAME_THREAD_SOURCES = ../../../libretro-common/queues/retro_spsc.c
FRAME_THREAD_FLAGS = -pthread
endif
all: test$(EXE) session_test$(EXE) runtime_test$(EXE) submit_test$(EXE) pool_test$(EXE) export_test$(EXE) identity_test$(EXE) prime_device_test$(EXE) prime_format_test$(EXE) prime_surface_test$(EXE) prime_image_test$(EXE) prime_frame_test$(EXE) prime_submit_test$(EXE) frame_test$(EXE) lease_test$(EXE) resources_test$(EXE) cache_test$(EXE) renderer_test$(EXE) decoder_test$(EXE) owner_test$(EXE) abi-check $(NATIVE_PROBE)
owner_test$(EXE): owner_test.c runtime_test.c session_test.c submit_test.c ../../../gfx/common/vaapi_hevc_owner.c ../../../gfx/common/vaapi_hevc_owner.h ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_hevc_decoder.h ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_runtime.h ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_export.h ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc.h
	$(CC) $(CFLAGS) owner_test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_export.c -o $@ $(LDFLAGS) -lm
decoder_test$(EXE): decoder_test.c pool_test.c test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_hevc_decoder.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc.h ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_export.h ../../../libretro-common/formats/h265/rh265.c ../../../libretro-common/include/formats/rh265_hw.h ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) -Wno-unused-parameter decoder_test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_export.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
renderer_test$(EXE): renderer_test.c cache_test.c ../../../gfx/drivers/vulkan_vaapi_video.h resources_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_cache.c ../../../gfx/common/vaapi_hevc_vulkan_cache.h ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../libretro-common/formats/image/image_yuv_blit.c ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) renderer_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c -o $@ $(LDFLAGS) -lm
cache_test$(EXE): cache_test.c resources_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_cache.c ../../../gfx/common/vaapi_hevc_vulkan_cache.h ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../libretro-common/formats/image/image_yuv_blit.c ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) cache_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c -o $@ $(LDFLAGS) -lm
resources_test$(EXE): resources_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) resources_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c -o $@ $(LDFLAGS)
runtime_test$(EXE): runtime_test.c session_test.c submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
pool_test$(EXE): pool_test.c test.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -Wno-unused-parameter pool_test.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
export_test$(EXE): export_test.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_export.h ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h
	$(CC) $(CFLAGS) export_test.c -o $@ $(LDFLAGS)
identity_test$(EXE): identity_test.c ../../../gfx/common/vulkan_drm_identity.h
	$(CC) $(CFLAGS) identity_test.c -o $@ $(LDFLAGS)
prime_device_test$(EXE): prime_device_test.c ../../../gfx/common/vulkan_prime_device.h ../../../gfx/common/vulkan_drm_identity.h
	$(CC) $(CFLAGS) prime_device_test.c -o $@ $(LDFLAGS)
prime_format_test$(EXE): prime_format_test.c ../../../gfx/common/vulkan_prime_format.h ../../../gfx/common/vulkan_drm_identity.h
	$(CC) $(CFLAGS) prime_format_test.c -o $@ $(LDFLAGS)
prime_surface_test$(EXE): prime_surface_test.c prime_format_test.c ../../../gfx/common/vaapi_hevc_vulkan.h ../../../gfx/common/vulkan_prime_format.h
	$(CC) $(CFLAGS) prime_surface_test.c -o $@ $(LDFLAGS)
prime_image_test$(EXE): prime_image_test.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../gfx/common/vulkan_prime_format.h
	$(CC) $(CFLAGS) prime_image_test.c ../../../gfx/common/vulkan_prime_image.c -o $@ $(LDFLAGS)
prime_frame_test$(EXE): prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../gfx/common/vulkan_prime_format.h ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_submit.c
	$(CC) $(CFLAGS) prime_frame_test.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c -o $@ $(LDFLAGS)
prime_submit_test$(EXE): prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../gfx/common/vulkan_video_import.h ../../../gfx/common/video_import_color.h ../../../libretro-common/formats/image/image_yuv_blit.c
	$(CC) $(CFLAGS) prime_submit_test.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c -o $@ $(LDFLAGS) -lm
prime_gpu_test$(EXE): prime_gpu_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../d3d_p010_import/fp16_reference.h ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc ../../../libretro-common/formats/image/image_yuv_blit.c
	$(CC) $(CFLAGS) prime_gpu_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c -o $@ $(LDFLAGS) -lvulkan -lm
gpu-check: prime_gpu_test$(EXE)
	$(RUNNER) ./prime_gpu_test$(EXE)
frame_test$(EXE): frame_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(CFLAGS) frame_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c $(FRAME_THREAD_SOURCES) -o $@ $(LDFLAGS) $(FRAME_THREAD_FLAGS)
lease_test$(EXE): lease_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(CFLAGS) lease_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c $(FRAME_THREAD_SOURCES) -o $@ $(LDFLAGS) $(FRAME_THREAD_FLAGS) -lm
frame_tsan_test$(EXE): frame_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -fsanitize=thread -fno-omit-frame-pointer frame_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c $(FRAME_THREAD_SOURCES) -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread $(FRAME_THREAD_FLAGS)
lease_tsan_test$(EXE): lease_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -fsanitize=thread -fno-omit-frame-pointer lease_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c $(FRAME_THREAD_SOURCES) -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread $(FRAME_THREAD_FLAGS) -lm
tsan-check: frame_tsan_test$(EXE) lease_tsan_test$(EXE)
	TSAN_OPTIONS=halt_on_error=1 $(RUNNER) ./frame_tsan_test$(EXE)
	TSAN_OPTIONS=halt_on_error=1 $(RUNNER) ./lease_tsan_test$(EXE)
native_probe: native_probe.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_session.c ../../../libretro-common/dynamic/dylib.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -ldl
session_test$(EXE): session_test.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
abi-check: abi.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) -fsyntax-only abi.c
test$(EXE): test.c ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc.h \
 ../../../libretro-common/include/formats/rh265.h \
 ../../../libretro-common/formats/h265/rh265.c \
 ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) -Wno-unused-parameter test.c ../../../libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./runtime_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
	$(RUNNER) ./export_test$(EXE)
	$(RUNNER) ./identity_test$(EXE)
	$(RUNNER) ./prime_device_test$(EXE)
	$(RUNNER) ./prime_format_test$(EXE)
	$(RUNNER) ./prime_surface_test$(EXE)
	$(RUNNER) ./prime_image_test$(EXE)
	$(RUNNER) ./prime_frame_test$(EXE)
	$(RUNNER) ./prime_submit_test$(EXE)
	$(RUNNER) ./frame_test$(EXE)
	$(RUNNER) ./lease_test$(EXE)
	$(RUNNER) ./resources_test$(EXE)
	$(RUNNER) ./cache_test$(EXE)
	$(RUNNER) ./renderer_test$(EXE)
	$(RUNNER) ./decoder_test$(EXE)
	$(RUNNER) ./owner_test$(EXE)
ifneq ($(NATIVE_PROBE),)
	./native_probe
endif
clean:
	rm -f owner_test owner_test.exe decoder_test decoder_test.exe renderer_test renderer_test.exe cache_test cache_test.exe test test.exe session_test session_test.exe runtime_test runtime_test.exe submit_test submit_test.exe pool_test pool_test.exe export_test export_test.exe identity_test identity_test.exe prime_device_test prime_device_test.exe prime_format_test prime_format_test.exe prime_surface_test prime_surface_test.exe prime_image_test prime_image_test.exe prime_frame_test prime_frame_test.exe prime_submit_test prime_submit_test.exe frame_test frame_test.exe lease_test lease_test.exe resources_test resources_test.exe lease_tsan_test lease_tsan_test.exe frame_tsan_test frame_tsan_test.exe prime_gpu_test prime_gpu_test.exe native_probe
.PHONY: all check gpu-check tsan-check clean abi-check
