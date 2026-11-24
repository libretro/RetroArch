CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: native_test$(EXE) params_test$(EXE) session_test$(EXE) picture_test$(EXE) submit_test$(EXE) pool_test$(EXE) encoder_test$(EXE) worker_test$(EXE) capture_test$(EXE) capture_shader_test$(EXE)
all: device_plan_test$(EXE) renderer_test$(EXE)
all: hevc_params_test$(EXE)
all: hevc_session_test$(EXE)
all: hevc_picture_test$(EXE) hevc_submit_test$(EXE)
all: hevc_encoder_test$(EXE) hevc_worker_test$(EXE)

hevc_encoder_test$(EXE): hevc_encoder_test.c $(RA)/gfx/common/vulkan_encode_hevc.c $(RA)/gfx/common/vulkan_encode_hevc.h $(RA)/gfx/common/vulkan_encode_hevc_submit.h $(RA)/gfx/common/vulkan_encode_hevc_picture.h
	$(CC) $(CFLAGS) hevc_encoder_test.c -o $@ $(LDFLAGS)
HEVC_WORKER_SOURCES := hevc_worker_test.c $(RA)/record/record_gpu_worker_vulkan.c \
   $(RA)/record/record_gpu_worker_vulkan_hevc.c $(RA)/record/record_gpu_queue.c \
   $(RA)/libretro-common/queues/retro_spsc.c
hevc_worker_test$(EXE): $(HEVC_WORKER_SOURCES) worker_test.c $(RA)/record/record_gpu_worker_vulkan_hevc.h $(RA)/record/record_gpu_worker_vulkan.h
	$(CC) $(CFLAGS) $(HEVC_WORKER_SOURCES) -o $@ $(LDFLAGS)

hevc_picture_test$(EXE): hevc_picture_test.c $(RA)/gfx/common/vulkan_encode_hevc_picture.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h265std_encode.h
	$(CC) $(CFLAGS) hevc_picture_test.c -o $@ $(LDFLAGS)
hevc_submit_test$(EXE): hevc_submit_test.c $(RA)/gfx/common/vulkan_encode_hevc_submit.c $(RA)/gfx/common/vulkan_encode_hevc_submit.h $(RA)/gfx/common/vulkan_encode_hevc_picture.h $(RA)/gfx/common/vulkan_encode_hevc_session.h $(RA)/gfx/common/vulkan_encode_hevc_params.h
	$(CC) $(CFLAGS) hevc_submit_test.c -o $@ $(LDFLAGS)

hevc_session_test$(EXE): hevc_session_test.c $(RA)/gfx/common/vulkan_encode_hevc_session.c $(RA)/gfx/common/vulkan_encode_hevc_session.h $(RA)/gfx/common/vulkan_encode_hevc_params.h
	$(CC) $(CFLAGS) hevc_session_test.c -o $@ $(LDFLAGS)

HEVC_NATIVE_SOURCES := hevc_native_params_test.c $(RA)/gfx/common/vulkan_encode_hevc_session.c \
   $(RA)/libretro-common/dynamic/dylib.c $(RA)/libretro-common/encodings/encoding_utf.c \
   $(RA)/libretro-common/compat/compat_strl.c
hevc_native_params_test$(EXE): $(HEVC_NATIVE_SOURCES) hevc_native_context.h $(RA)/gfx/common/vulkan_encode_hevc_session.h $(RA)/gfx/common/vulkan_encode_hevc_params.h
	$(CC) $(CFLAGS) -DHAVE_DYLIB $(HEVC_NATIVE_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl)
hevc_headers_test$(EXE): hevc_headers_test.c $(RA)/libretro-common/formats/h265/rh265.c $(RA)/libretro-common/include/formats/rh265.h
	$(CC) $(CFLAGS) -Wno-unused-parameter hevc_headers_test.c $(RA)/libretro-common/formats/h265/rh265.c -o $@ $(LDFLAGS) -lm
hevc-native-params-check: hevc_native_params_test$(EXE) hevc_headers_test$(EXE)
	python3 hevc_native_params_run.py --native-test ./hevc_native_params_test$(EXE) --headers-test ./hevc_headers_test$(EXE)
.PHONY: hevc-native-params-check
HEVC_ENCODE_SOURCES := $(filter-out hevc_native_params_test.c,$(HEVC_NATIVE_SOURCES)) \
   hevc_native_encode_test.c $(RA)/gfx/common/vulkan_encode_hevc_submit.c $(RA)/gfx/common/vulkan_encode_pool.c
hevc_native_encode_test$(EXE): $(HEVC_ENCODE_SOURCES) hevc_native_context.h $(RA)/gfx/common/vulkan_encode_hevc_submit.h $(RA)/gfx/common/vulkan_encode_hevc_picture.h $(RA)/gfx/common/vulkan_encode_hevc_params.h $(RA)/gfx/common/vulkan_encode_pool.h
	$(CC) $(CFLAGS) -DHAVE_DYLIB $(HEVC_ENCODE_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl)
hevc-native-check: hevc_native_encode_test$(EXE)
	python3 hevc_native_encode_run.py --native-test ./hevc_native_encode_test$(EXE)
.PHONY: hevc-native-check
HEVC_ADAPTER_SOURCES := $(filter-out hevc_native_encode_test.c,$(HEVC_ENCODE_SOURCES)) \
   hevc_native_adapter_test.c $(RA)/gfx/common/vulkan_encode_hevc.c
hevc_native_adapter_test$(EXE): $(HEVC_ADAPTER_SOURCES) hevc_native_encode_test.c hevc_native_context.h $(RA)/gfx/common/vulkan_encode_hevc.h $(RA)/gfx/common/vulkan_encode_hevc_picture.h
	$(CC) $(CFLAGS) -DHAVE_DYLIB $(HEVC_ADAPTER_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl)
hevc-native-adapter-check: hevc_native_adapter_test$(EXE)
	python3 hevc_native_encode_run.py --native-test ./hevc_native_adapter_test$(EXE) --with-cancel
HEVC_NATIVE_WORKER_SOURCES := $(filter-out hevc_native_adapter_test.c,$(HEVC_ADAPTER_SOURCES)) \
   hevc_native_worker_test.c $(filter-out hevc_worker_test.c,$(HEVC_WORKER_SOURCES)) \
   $(RA)/gfx/common/vulkan_encode_h264.c $(RA)/gfx/common/vulkan_encode_h264_session.c \
   $(RA)/gfx/common/vulkan_encode_h264_submit.c
hevc_native_worker_test$(EXE): $(HEVC_NATIVE_WORKER_SOURCES) hevc_native_adapter_test.c hevc_native_encode_test.c hevc_native_context.h $(RA)/record/record_gpu_worker_vulkan_hevc.h $(RA)/record/record_gpu_worker_vulkan.h
	$(CC) $(CFLAGS) -DHAVE_DYLIB $(HEVC_NATIVE_WORKER_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl)
hevc-native-worker-check: hevc_native_worker_test$(EXE)
	python3 hevc_native_encode_run.py --native-test ./hevc_native_worker_test$(EXE) --with-cancel
.PHONY: hevc-native-adapter-check hevc-native-worker-check

native_test$(EXE) encoder_test$(EXE) worker_test$(EXE) hevc_encoder_test$(EXE) hevc_worker_test$(EXE) hevc_native_adapter_test$(EXE) hevc_native_worker_test$(EXE): $(RA)/gfx/common/vulkan_encode_packet.h

hevc_params_test$(EXE): hevc_params_test.c $(RA)/gfx/common/vulkan_encode_hevc_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h265std.h
	$(CC) $(CFLAGS) hevc_params_test.c -o $@ $(LDFLAGS)

renderer_test$(EXE): renderer_test.c $(RA)/record/record_gpu_renderer_vulkan.c $(RA)/record/record_gpu_renderer_vulkan.h $(RA)/record/record_gpu_dispatch_vulkan.h $(RA)/record/record_gpu_session_vulkan.h $(RA)/gfx/common/vulkan_encode_capture.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/record/record_gpu_queue.h
	$(CC) $(CFLAGS) renderer_test.c -o $@ $(LDFLAGS)
device_plan_test$(EXE): device_plan_test.c $(RA)/gfx/common/vulkan_encode_device.c $(RA)/gfx/common/vulkan_encode_device.h
	$(CC) $(CFLAGS) device_plan_test.c -o $@ $(LDFLAGS)
params_test$(EXE): params_test.c $(RA)/gfx/common/vulkan_encode_h264_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std.h
	$(CC) $(CFLAGS) params_test.c -o $@ $(LDFLAGS)
session_test$(EXE): session_test.c $(RA)/gfx/common/vulkan_encode_h264_session.c $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
picture_test$(EXE): picture_test.c $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std_encode.h
	$(CC) $(CFLAGS) picture_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c $(RA)/gfx/common/vulkan_encode_h264_submit.c $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std_encode.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
pool_test$(EXE): pool_test.c $(RA)/gfx/common/vulkan_encode_pool.c $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/include/vulkan/vulkan.h
	$(CC) $(CFLAGS) pool_test.c -o $@ $(LDFLAGS)
encoder_test$(EXE): encoder_test.c $(RA)/gfx/common/vulkan_encode_h264.c $(RA)/gfx/common/vulkan_encode_h264.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) encoder_test.c -o $@ $(LDFLAGS)
worker_test$(EXE): worker_test.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_worker_vulkan.h $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_queue.h $(RA)/libretro-common/queues/retro_spsc.c $(RA)/gfx/common/vulkan_encode_h264.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) worker_test.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c -o $@ $(LDFLAGS)
CAPTURE_HEADERS := $(RA)/gfx/common/vulkan_encode_capture.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_capture_p010.h
CAPTURE_SHADER_SOURCES := capture_shader_test.c $(RA)/gfx/common/vulkan_encode_capture_shader.c \
   $(RA)/libretro-common/gfx/shaders/rslc_front.c \
   $(RA)/libretro-common/gfx/shaders/rslc_ir.c \
   $(RA)/libretro-common/gfx/shaders/rslc_emit.c
capture_test$(EXE): capture_test.c $(RA)/gfx/common/vulkan_encode_capture.c $(CAPTURE_HEADERS)
	$(CC) $(CFLAGS) capture_test.c -o $@ $(LDFLAGS)
capture_shader_test$(EXE): $(CAPTURE_SHADER_SOURCES) $(CAPTURE_HEADERS) $(RA)/libretro-common/include/gfx/shaders/rslc.h $(RA)/libretro-common/gfx/shaders/rslc_share.h
	$(CC) $(CFLAGS) -DRSLC_EXTENDED -DRSLC_NO_DXBC $(CAPTURE_SHADER_SOURCES) -o $@ $(LDFLAGS) -lm
check: all
	python3 instance_extensions_test.py --cc "$(CC)" --runner "$(RUNNER)"
	$(RUNNER) ./hevc_params_test$(EXE)
	$(RUNNER) ./hevc_session_test$(EXE)
	$(RUNNER) ./hevc_picture_test$(EXE)
	$(RUNNER) ./hevc_submit_test$(EXE)
	$(RUNNER) ./hevc_encoder_test$(EXE)
	$(RUNNER) ./hevc_worker_test$(EXE)
	$(RUNNER) ./renderer_test$(EXE)
	$(RUNNER) ./device_plan_test$(EXE)
	python3 device_wrapper_test.py --cc "$(CC)" --sanitizer "$(SANITIZER)" --runner "$(RUNNER)"
	$(RUNNER) ./params_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./picture_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
	$(RUNNER) ./encoder_test$(EXE)
	$(RUNNER) ./worker_test$(EXE)
	$(RUNNER) ./capture_test$(EXE)
	$(RUNNER) ./capture_shader_test$(EXE)
clean:
	rm -f hevc_native_stream_test.exe
	rm -f hevc_native_hdr_test hevc_native_hdr_test.exe
	rm -f hevc_native_dispatch_test hevc_native_dispatch_test.exe
	rm -f hevc_native_session_test hevc_native_session_test.exe
	rm -f hevc_native_capture_test hevc_native_capture_test.exe
	rm -f hevc_params_test hevc_params_test.exe
	rm -f hevc_session_test hevc_session_test.exe hevc_native_params_test hevc_native_params_test.exe hevc_headers_test hevc_headers_test.exe
	rm -f hevc_picture_test hevc_picture_test.exe hevc_submit_test hevc_submit_test.exe hevc_native_encode_test hevc_native_encode_test.exe
	rm -f hevc_encoder_test hevc_encoder_test.exe hevc_worker_test hevc_worker_test.exe hevc_native_adapter_test hevc_native_adapter_test.exe hevc_native_worker_test hevc_native_worker_test.exe
	rm -f renderer_test renderer_test.exe
	rm -f device_plan_test device_plan_test.exe
	rm -f native_test native_test.exe
	rm -f params_test params_test.exe session_test session_test.exe picture_test picture_test.exe submit_test submit_test.exe pool_test pool_test.exe encoder_test encoder_test.exe worker_test worker_test.exe capture_test capture_test.exe capture_shader_test capture_shader_test.exe
.PHONY: all check clean

NATIVE_SOURCES := native_test.c $(RA)/gfx/common/vulkan_encode_h264.c \
   $(RA)/gfx/common/vulkan_encode_h264_session.c $(RA)/gfx/common/vulkan_encode_h264_submit.c \
   $(RA)/gfx/common/vulkan_encode_pool.c $(RA)/libretro-common/dynamic/dylib.c \
   $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c
NATIVE_SOURCES += $(RA)/gfx/common/vulkan_encode_capture.c $(RA)/gfx/common/vulkan_encode_capture_shader.c \
   $(RA)/libretro-common/gfx/shaders/rslc_front.c $(RA)/libretro-common/gfx/shaders/rslc_ir.c \
   $(RA)/libretro-common/gfx/shaders/rslc_emit.c
NATIVE_SOURCES += $(RA)/record/record_gpu_session_vulkan.c $(RA)/record/record_gpu_worker_vulkan.c \
   $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_output_vulkan.c $(RA)/record/record_mux_mp4.c \
   $(RA)/record/record_file_posix.c $(RA)/record/record_mediacodec_file.c \
   $(RA)/libretro-common/queues/retro_spsc.c $(RA)/libretro-common/formats/ac3/rac3_frame.c \
   $(RA)/libretro-common/formats/ac3/rac3_encode.c
NATIVE_SOURCES += $(RA)/record/record_gpu_dispatch_vulkan.c ../../record/gpu_av_output/rthreads_compat.c $(RA)/libretro-common/rthreads/retro_eventcount.c
NATIVE_SOURCES += $(RA)/gfx/common/vulkan_encode_device.c
NATIVE_SOURCES += $(RA)/record/record_gpu_renderer_vulkan.c
native_test$(EXE): $(NATIVE_SOURCES) native_capture.h native_session.h $(RA)/record/record_gpu_dispatch_vulkan.h $(RA)/record/record_gpu_renderer_vulkan.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -DHAVE_DYLIB -DRSLC_EXTENDED -DRSLC_NO_DXBC $(if $(filter .exe,$(EXE)),-DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST,) $(NATIVE_SOURCES) -o $@ $(LDFLAGS) $(if $(filter Windows_NT,$(OS)),,-pthread) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl) -lm
native-check: native_test$(EXE)
	$(RUNNER) ./native_test$(EXE)
.PHONY: native-check

native_test$(EXE): $(CAPTURE_HEADERS)

HEVC_CAPTURE_SOURCES := $(filter-out hevc_native_adapter_test.c,$(HEVC_ADAPTER_SOURCES)) \
   hevc_native_capture_test.c $(RA)/gfx/common/vulkan_encode_capture.c \
   $(filter-out capture_shader_test.c,$(CAPTURE_SHADER_SOURCES))
hevc_native_capture_test$(EXE): $(HEVC_CAPTURE_SOURCES) hevc_native_adapter_test.c hevc_native_encode_test.c hevc_native_context.h $(CAPTURE_HEADERS)
	$(CC) $(CFLAGS) -DHAVE_DYLIB -DRSLC_EXTENDED -DRSLC_NO_DXBC $(HEVC_CAPTURE_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl) -lm
hevc-native-capture-check: hevc_native_capture_test$(EXE)
	python3 hevc_native_capture_run.py --native-test ./hevc_native_capture_test$(EXE)
.PHONY: hevc-native-capture-check

HEVC_NATIVE_SESSION_SOURCES := $(sort $(filter-out native_test.c hevc_native_capture_test.c,$(NATIVE_SOURCES) $(HEVC_CAPTURE_SOURCES)) hevc_native_session_test.c $(RA)/record/record_gpu_session_vulkan_hevc.c $(RA)/record/record_gpu_worker_vulkan_hevc.c)
hevc_native_session_test$(EXE): $(HEVC_NATIVE_SESSION_SOURCES) hevc_native_capture_test.c hevc_native_adapter_test.c hevc_native_encode_test.c hevc_native_context.h $(CAPTURE_HEADERS) $(RA)/record/record_gpu_session_vulkan_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -DHAVE_DYLIB -DRSLC_EXTENDED -DRSLC_NO_DXBC $(if $(filter .exe,$(EXE)),-DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST,) $(HEVC_NATIVE_SESSION_SOURCES) -o $@ $(LDFLAGS) $(if $(filter Windows_NT,$(OS)),,-pthread) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl) -lm

hevc-native-session-check: hevc_native_session_test$(EXE)
	python3 hevc_native_session_run.py --native-test ./hevc_native_session_test$(EXE)
.PHONY: hevc-native-session-check

HEVC_NATIVE_DISPATCH_SOURCES := $(filter-out hevc_native_session_test.c,$(HEVC_NATIVE_SESSION_SOURCES)) hevc_native_dispatch_test.c $(RA)/record/record_gpu_dispatch_vulkan_hevc.c
hevc_native_dispatch_test$(EXE): $(HEVC_NATIVE_DISPATCH_SOURCES) hevc_native_capture_test.c hevc_native_adapter_test.c hevc_native_encode_test.c hevc_native_context.h $(CAPTURE_HEADERS) $(RA)/record/record_gpu_dispatch_vulkan_backend.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -DHAVE_DYLIB -DRSLC_EXTENDED -DRSLC_NO_DXBC $(if $(filter .exe,$(EXE)),-DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST,) $(HEVC_NATIVE_DISPATCH_SOURCES) -o $@ $(LDFLAGS) $(if $(filter Windows_NT,$(OS)),,-pthread) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl) -lm

hevc-native-dispatch-check: hevc_native_dispatch_test$(EXE)
	python3 hevc_native_session_run.py --native-test ./hevc_native_dispatch_test$(EXE) --with-cancel
.PHONY: hevc-native-dispatch-check
hevc_native_session_test$(EXE) hevc_native_dispatch_test$(EXE) native_test$(EXE): $(RA)/record/record_gpu_dispatch_vulkan_backend.h $(RA)/record/record_gpu_dispatch_vulkan_hevc.h

HEVC_NATIVE_HDR_SOURCES := $(filter-out hevc_native_capture_test.c,$(HEVC_CAPTURE_SOURCES)) hevc_native_hdr_test.c
hevc_native_hdr_test$(EXE): $(HEVC_NATIVE_HDR_SOURCES) hevc_native_capture_test.c hevc_native_adapter_test.c hevc_native_encode_test.c hevc_native_context.h $(CAPTURE_HEADERS)
	$(CC) $(CFLAGS) -DHAVE_DYLIB -DRSLC_EXTENDED -DRSLC_NO_DXBC $(HEVC_NATIVE_HDR_SOURCES) -o $@ $(LDFLAGS) $(if $(or $(filter Windows_NT,$(OS)),$(filter .exe,$(EXE))),,-ldl) -lm

hevc-native-hdr-check: hevc_native_hdr_test$(EXE)
	python3 hevc_native_capture_run.py --native-test ./hevc_native_hdr_test$(EXE) --hdr-mode 1
	python3 hevc_native_capture_run.py --native-test ./hevc_native_hdr_test$(EXE) --hdr-mode 1 --full
	python3 hevc_native_capture_run.py --native-test ./hevc_native_hdr_test$(EXE) --hdr-mode 2 --negative
	python3 hevc_native_capture_run.py --native-test ./hevc_native_hdr_test$(EXE) --hdr-mode 2 --full --negative
.PHONY: hevc-native-hdr-check

LRC := $(RA)/libretro-common
STREAM_NET := $(wildcard $(LRC)/net/net_rtmp_*.c) $(LRC)/net/net_resolve.c $(LRC)/net/net_socket.c $(LRC)/net/net_compat.c
STREAM_COMMON := $(LRC)/features/features_cpu.c $(LRC)/string/stdstring.c $(LRC)/string/rstrtod.c $(LRC)/compat/compat_posix_string.c $(LRC)/time/rtime.c $(LRC)/rthreads/rthreads.c $(LRC)/rthreads/retro_eventcount.c $(LRC)/crypto/kdf.c $(LRC)/formats/flv/rflv_mux.c
STREAM_COMMON += $(LRC)/crypto/crypto.c $(LRC)/hash/lrc_hash.c $(LRC)/utils/md5.c
STREAM_COMMON += $(LRC)/net/net_socket_ssl_retro.c $(LRC)/crypto/pk.c $(LRC)/crypto/x509.c $(LRC)/encodings/encoding_base64.c $(LRC)/streams/file_stream.c $(LRC)/vfs/vfs_implementation.c $(LRC)/file/file_path.c $(LRC)/file/file_path_io.c $(LRC)/compat/fopen_utf8.c
HEVC_STREAM_SOURCES := $(filter-out hevc_native_capture_test.c,$(HEVC_CAPTURE_SOURCES)) hevc_native_stream_test.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_worker_vulkan_hevc.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_stream_gpu.c $(RA)/record/record_aac_audio.c $(RA)/record/record_mf_audio.c $(RA)/record/record_gpu_dispatch_vulkan.c $(RA)/record/record_gpu_session_vulkan.c $(RA)/record/record_gpu_output_vulkan.c $(RA)/record/record_mux_mp4.c $(RA)/record/record_file_posix.c $(RA)/record/record_mediacodec_file.c $(LRC)/formats/ac3/rac3_frame.c $(LRC)/formats/ac3/rac3_encode.c $(RA)/gfx/common/vulkan_encode_h264.c $(RA)/gfx/common/vulkan_encode_h264_session.c $(RA)/gfx/common/vulkan_encode_h264_submit.c $(LRC)/queues/retro_spsc.c $(STREAM_NET) $(STREAM_COMMON)
HEVC_STREAM_HEADERS := $(RA)/record/record_aac_audio.h $(RA)/record/record_mf_audio.h $(RA)/record/record_gpu_dispatch_vulkan.h $(RA)/record/record_gpu_dispatch_vulkan_backend.h $(RA)/record/record_gpu_output_vulkan.h $(RA)/record/record_gpu_output_config.h $(RA)/record/record_stream_gpu.h $(RA)/record/record_gpu_session_vulkan.h $(RA)/record/record_gpu_session_vulkan_hevc.h
hevc_native_stream_test.exe: $(HEVC_STREAM_SOURCES) $(HEVC_STREAM_HEADERS) $(RA)/record/record_gpu_session_vulkan_hevc.c $(RA)/record/record_gpu_session_vulkan_backend.h hevc_native_capture_test.c $(RA)/record/record_stream_vulkan.c $(RA)/record/record_stream_vulkan.h
	$(CC) $(filter-out -Werror -std=c89 -pedantic,$(CFLAGS)) -std=gnu89 -Werror=declaration-after-statement -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -DHAVE_CRYPTO -DHAVE_SSL -DHAVE_RETROSSL -D_WIN32_WINNT=0x0601 -DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST -DRSLC_EXTENDED -DRSLC_NO_DXBC $(HEVC_STREAM_SOURCES) -o $@ $(LDFLAGS) -lm -lws2_32
hevc-native-stream-check: hevc_native_stream_test.exe
	python3 hevc_native_stream_run.py --native-test ./hevc_native_stream_test.exe
.PHONY: hevc-native-stream-check
