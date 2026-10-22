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
endif
all: test$(EXE) session_test$(EXE) runtime_test$(EXE) submit_test$(EXE) pool_test$(EXE) export_test$(EXE) identity_test$(EXE) prime_device_test$(EXE) prime_format_test$(EXE) prime_surface_test$(EXE) prime_image_test$(EXE) abi-check $(NATIVE_PROBE)
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
ifneq ($(NATIVE_PROBE),)
	./native_probe
endif
clean:
	rm -f test test.exe session_test session_test.exe runtime_test runtime_test.exe submit_test submit_test.exe pool_test pool_test.exe export_test export_test.exe identity_test identity_test.exe prime_device_test prime_device_test.exe prime_format_test prime_format_test.exe prime_surface_test prime_surface_test.exe prime_image_test prime_image_test.exe native_probe
.PHONY: all check clean abi-check
