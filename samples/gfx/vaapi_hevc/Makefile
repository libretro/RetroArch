CC ?= cc
ifeq ($(OS),Windows_NT)
BACKEND_HOST_FLAGS := -Wno-unused-parameter
endif
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
NATIVE_DECODE = native_decode
NATIVE_IMPORT = native_import
FRAME_THREAD_SOURCES = ../../../libretro-common/queues/retro_spsc.c
FRAME_THREAD_FLAGS = -pthread
endif
all: backend_nothreads_test$(EXE) backend_volatile_test$(EXE) node_test$(EXE) backend_test$(EXE) test$(EXE) session_test$(EXE) runtime_test$(EXE) submit_test$(EXE) pool_test$(EXE) export_test$(EXE) identity_test$(EXE) prime_device_test$(EXE) prime_format_test$(EXE) prime_surface_test$(EXE) prime_image_test$(EXE) prime_frame_test$(EXE) prime_submit_test$(EXE) frame_test$(EXE) lease_test$(EXE) resources_test$(EXE) cache_test$(EXE) renderer_test$(EXE) decoder_test$(EXE) owner_test$(EXE) abi-check $(NATIVE_PROBE)
backend_nothreads_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_hevc_backend.c ../../../gfx/common/vaapi_hevc_backend.h
	$(CC) $(CFLAGS) backend_disabled_test.c ../../../gfx/common/vaapi_hevc_backend.c -o $@ $(LDFLAGS) -lm
backend_volatile_test$(EXE): backend_disabled_test.c ../../../gfx/common/vaapi_hevc_backend.c ../../../gfx/common/vaapi_hevc_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -DRETRO_ATOMIC_FORCE_VOLATILE -DRETRO_ATOMIC_SUPPRESS_WARNING backend_disabled_test.c ../../../gfx/common/vaapi_hevc_backend.c -o $@ $(LDFLAGS) -lm
node_test$(EXE): node_test.c ../../../gfx/common/vaapi_hevc_backend.c ../../../gfx/common/vaapi_hevc_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -ffunction-sections -fdata-sections node_test.c -o $@ $(LDFLAGS) -Wl,--gc-sections
BACKEND_SOURCES := ../../../libretro-common/rthreads/rthreads.c \
 ../../../libretro-common/rthreads/retro_eventcount.c
backend_test$(EXE): backend_test.c ../../../gfx/common/vaapi_hevc_backend.c ../../../gfx/common/vaapi_hevc_backend.h ../../../gfx/drivers/vulkan_vaapi_backend.h ../../../gfx/common/vulkan_drm_identity.h $(BACKEND_SOURCES)
	$(CC) $(CFLAGS) $(BACKEND_HOST_FLAGS) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread backend_test.c $(BACKEND_SOURCES) -o $@ $(LDFLAGS) -lm
backend_tsan_test$(EXE): backend_test.c ../../../gfx/common/vaapi_hevc_backend.c ../../../gfx/common/vaapi_hevc_backend.h ../../../gfx/drivers/vulkan_vaapi_backend.h ../../../gfx/common/vulkan_drm_identity.h $(BACKEND_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) $(BACKEND_HOST_FLAGS) -DHAVE_THREADS -D_DEFAULT_SOURCE -pthread -fsanitize=thread -fno-omit-frame-pointer backend_test.c $(BACKEND_SOURCES) -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread -lm
owner_test$(EXE): owner_test.c runtime_test.c session_test.c submit_test.c ../../../gfx/common/vaapi_hevc_owner.c ../../../gfx/common/vaapi_hevc_owner.h ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_hevc_decoder.h ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_runtime.h ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_export.h ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc.h
	$(CC) $(CFLAGS) owner_test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_export.c rh265_test.o -o $@ $(LDFLAGS) -lm
decoder_test$(EXE): decoder_test.c pool_test.c test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_hevc_decoder.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc.h ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_export.h ../../../libretro-common/formats/h265/rh265.c ../../../libretro-common/include/formats/rh265_hw.h ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) -Wno-unused-parameter decoder_test.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_export.c rh265_test.o -o $@ $(LDFLAGS) -lm
renderer_test$(EXE): renderer_test.c cache_test.c ../../../gfx/drivers/vulkan_vaapi_video.h resources_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_cache.c ../../../gfx/common/vaapi_hevc_vulkan_cache.h ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../libretro-common/formats/image/image_yuv_blit.c ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) renderer_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c rh265_test.o -o $@ $(LDFLAGS) -lm
cache_test$(EXE): cache_test.c resources_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_cache.c ../../../gfx/common/vaapi_hevc_vulkan_cache.h ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../libretro-common/formats/image/image_yuv_blit.c ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) cache_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c rh265_test.o -o $@ $(LDFLAGS) -lm
resources_test$(EXE): resources_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc
	$(CC) $(CFLAGS) resources_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c -o $@ $(LDFLAGS)
runtime_test$(EXE): runtime_test.c session_test.c submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_runtime.h ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_session.h
	$(CC) $(CFLAGS) runtime_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
pool_test$(EXE): pool_test.c test.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_packet.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h ../../../libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -Wno-unused-parameter pool_test.c rh265_test.o -o $@ $(LDFLAGS) -lm
export_test$(EXE): export_test.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_export.h ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_pool.h ../../../gfx/common/vaapi_hevc_submit.c ../../../gfx/common/vaapi_hevc_submit.h
	$(CC) $(CFLAGS) export_test.c rh265_test.o -o $@ $(LDFLAGS)
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
	$(CC) $(CFLAGS) prime_frame_test.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c rh265_test.o -o $@ $(LDFLAGS)
prime_submit_test$(EXE): prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../../../gfx/common/vulkan_video_import.h ../../../gfx/common/video_import_color.h ../../../libretro-common/formats/image/image_yuv_blit.c
	$(CC) $(CFLAGS) prime_submit_test.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c rh265_test.o -o $@ $(LDFLAGS) -lm
prime_gpu_test$(EXE): prime_gpu_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vaapi_hevc_vulkan_resources.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h ../d3d_p010_import/fp16_reference.h ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc ../../../gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc ../../../libretro-common/formats/image/image_yuv_blit.c
	$(CC) $(CFLAGS) prime_gpu_test.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c -o $@ $(LDFLAGS) -lvulkan -lm
gpu-check: prime_gpu_test$(EXE)
	$(RUNNER) ./prime_gpu_test$(EXE)
	$(RUNNER) ./native_import_gpu_test$(EXE)
frame_test$(EXE): frame_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(CFLAGS) frame_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c $(FRAME_THREAD_SOURCES) rh265_test.o -o $@ $(LDFLAGS) $(FRAME_THREAD_FLAGS)
lease_test$(EXE): lease_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(CFLAGS) lease_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c $(FRAME_THREAD_SOURCES) rh265_test.o -o $@ $(LDFLAGS) $(FRAME_THREAD_FLAGS) -lm
frame_tsan_test$(EXE): frame_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -fsanitize=thread -fno-omit-frame-pointer frame_test.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c $(FRAME_THREAD_SOURCES) rh265_tsan.o -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread $(FRAME_THREAD_FLAGS)
lease_tsan_test$(EXE): lease_test.c prime_submit_test.c prime_frame_test.c export_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_hevc_vulkan_lease.h ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_frame.h ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_frame.h ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_submit.h ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vulkan_prime_image.h $(FRAME_THREAD_SOURCES)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -fsanitize=thread -fno-omit-frame-pointer lease_test.c ../../../gfx/common/vaapi_hevc_vulkan_lease.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../libretro-common/formats/image/image_yuv_blit.c $(FRAME_THREAD_SOURCES) rh265_tsan.o -o $@ $(filter-out -fsanitize=%,$(LDFLAGS)) -fsanitize=thread $(FRAME_THREAD_FLAGS) -lm
tsan-check: backend_tsan_test$(EXE) frame_tsan_test$(EXE) lease_tsan_test$(EXE)
	TSAN_OPTIONS=halt_on_error=1 $(RUNNER) ./backend_tsan_test$(EXE)
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
	$(CC) $(CFLAGS) -Wno-unused-parameter test.c rh265_test.o -o $@ $(LDFLAGS) -lm
long_slice_test$(EXE): long_slice_test.c ../../../gfx/common/vaapi_hevc_slice.h ../../../gfx/common/vaapi_hevc.h
	$(CC) $(CFLAGS) long_slice_test.c -o $@ $(LDFLAGS)
long_packet_test$(EXE): long_packet_test.c ../../../gfx/common/vaapi_hevc_long_packet.h ../../../gfx/common/vaapi_hevc_slice.h ../../../libretro-common/samples/formats/h265/rh265_slice_info_test.c ../../../libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -Wno-unused-parameter long_packet_test.c -o $@ $(LDFLAGS) -lm
all: long_slice_test$(EXE) long_packet_test$(EXE)
test$(EXE): ../../../gfx/common/vaapi_hevc_slice.h
test$(EXE): ../../../gfx/common/vaapi_hevc_long_packet.h
check: all
	$(RUNNER) ./weighted_test$(EXE)
	$(RUNNER) ./native_pixels_test$(EXE)
	$(RUNNER) ./long_packet_test$(EXE)
	$(RUNNER) ./long_slice_test$(EXE)
	$(RUNNER) ./backend_nothreads_test$(EXE)
	$(RUNNER) ./backend_volatile_test$(EXE)
	$(RUNNER) ./node_test$(EXE)
	$(RUNNER) ./backend_test$(EXE)
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
	rm -f native_import_gpu_test native_import_gpu_test.exe
	rm -f weighted_test weighted_test.exe
	rm -f native_import native_decode native_pixels_test native_pixels_test.exe
	rm -f rh265_test.o rh265_tsan.o
	rm -f long_packet_test long_packet_test.exe
	rm -f long_slice_test long_slice_test.exe
	rm -f backend_nothreads_test backend_nothreads_test.exe backend_volatile_test backend_volatile_test.exe node_test node_test.exe backend_test backend_test.exe backend_tsan_test backend_tsan_test.exe owner_test owner_test.exe decoder_test decoder_test.exe renderer_test renderer_test.exe cache_test cache_test.exe test test.exe session_test session_test.exe runtime_test runtime_test.exe submit_test submit_test.exe pool_test pool_test.exe export_test export_test.exe identity_test identity_test.exe prime_device_test prime_device_test.exe prime_format_test prime_format_test.exe prime_surface_test prime_surface_test.exe prime_image_test prime_image_test.exe prime_frame_test prime_frame_test.exe prime_submit_test prime_submit_test.exe frame_test frame_test.exe lease_test lease_test.exe resources_test resources_test.exe lease_tsan_test lease_tsan_test.exe frame_tsan_test frame_tsan_test.exe prime_gpu_test prime_gpu_test.exe native_probe
.PHONY: all check gpu-check tsan-check clean abi-check

HEVC_PARSER := ../../../libretro-common/formats/h265/rh265.c
HEVC_PARSER_DEPS := $(HEVC_PARSER) ../../../libretro-common/formats/h265/rh265_bd.inc ../../../libretro-common/include/formats/rh265.h ../../../libretro-common/include/formats/rh265_hw.h
rh265_test.o: $(HEVC_PARSER_DEPS)
	$(CC) $(CFLAGS) -Wno-unused-parameter -c $(HEVC_PARSER) -o $@
rh265_tsan.o: $(HEVC_PARSER_DEPS)
	$(CC) $(filter-out -fsanitize=%,$(CFLAGS)) -Wno-unused-parameter -fsanitize=thread -fno-omit-frame-pointer -c $(HEVC_PARSER) -o $@
test$(EXE) pool_test$(EXE) decoder_test$(EXE) owner_test$(EXE) export_test$(EXE) prime_frame_test$(EXE) prime_submit_test$(EXE) frame_test$(EXE) lease_test$(EXE) cache_test$(EXE) renderer_test$(EXE): rh265_test.o ../../../gfx/common/vaapi_hevc_long_packet.h ../../../gfx/common/vaapi_hevc_slice.h
native_probe: rh265_test.o
frame_tsan_test$(EXE) lease_tsan_test$(EXE): rh265_tsan.o

native_pixels_test$(EXE): native_pixels_test.c native_pixels.h native_rgb.h
	$(CC) $(CFLAGS) native_pixels_test.c -o $@ $(LDFLAGS)
all: native_pixels_test$(EXE) $(NATIVE_DECODE) $(NATIVE_IMPORT)
NATIVE_DECODE_SOURCES := native_decode.c ../../../gfx/common/vaapi_hevc_owner.c ../../../gfx/common/vaapi_hevc_decoder.c ../../../gfx/common/vaapi_frame.c ../../../gfx/common/vaapi_hevc_pool.c ../../../gfx/common/vaapi_hevc_export.c ../../../gfx/common/vaapi_hevc_runtime.c ../../../gfx/common/vaapi_hevc_session.c ../../../gfx/common/vaapi_hevc_submit.c ../../../libretro-common/dynamic/dylib.c
native_decode: $(NATIVE_DECODE_SOURCES) rh265_test.o native_pixels.h ../videotoolbox_hevc/sequence_data.h ../videotoolbox_hevc/reorder_data.h
	$(CC) $(CFLAGS) $(NATIVE_DECODE_SOURCES) rh265_test.o -o $@ $(LDFLAGS) -ldl -lva
native-decode-check: native_decode
	./native_decode "$(VAAPI_RENDER_NODE)"
.PHONY: native-decode-check
weighted_test$(EXE): weighted_test.c test.c weighted_data.h native_pixels.h rh265_test.o
	$(CC) $(CFLAGS) -Wno-unused-parameter weighted_test.c rh265_test.o -o $@ $(LDFLAGS) -lm
all: weighted_test$(EXE)
native_decode: weighted_data.h

NATIVE_IMPORT_SOURCES := ../../../gfx/common/vulkan_prime_image.c ../../../gfx/common/vaapi_hevc_vulkan_frame.c ../../../gfx/common/vaapi_hevc_vulkan_submit.c ../../../gfx/common/vaapi_hevc_vulkan_resources.c ../../../libretro-common/formats/image/image_yuv_blit.c
native_import: $(NATIVE_DECODE_SOURCES) $(NATIVE_IMPORT_SOURCES) native_import.h native_import_gpu.h native_rgb.h native_pixels.h weighted_data.h rh265_test.o
	$(CC) $(CFLAGS) -DNATIVE_VULKAN_IMPORT $(NATIVE_DECODE_SOURCES) $(NATIVE_IMPORT_SOURCES) rh265_test.o -o $@ $(LDFLAGS) -ldl -lva -lvulkan -ldrm
native-import-check: native_import
	./native_import "$(VAAPI_RENDER_NODE)"
.PHONY: native-import-check
native_import_gpu_test$(EXE): native_import_gpu_test.c native_import_gpu.h native_rgb.h prime_gpu_test$(EXE)
	$(CC) $(CFLAGS) native_import_gpu_test.c $(NATIVE_IMPORT_SOURCES) -o $@ $(LDFLAGS) -lvulkan -lm
gpu-check: native_import_gpu_test$(EXE)
