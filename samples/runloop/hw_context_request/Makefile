TARGET := hw_context_request_test
ROOT := ../../..
SOURCES := hw_context_request_test.c $(ROOT)/libretro-common/compat/compat_strl.c
CFLAGS += -O2 -g1 -std=gnu89 -Wall -flto \
          -ffunction-sections -fdata-sections \
          -I$(ROOT) -I$(ROOT)/menu -I$(ROOT)/libretro-common/include $(EXTRA_CFLAGS)
LDFLAGS += -flto -Wl,--gc-sections
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

GLES := -DHAVE_OPENGL -DHAVE_OPENGLES
# The regular Android flavors: GLES2 headers, device property read.
# The stub directory stands in for the NDK's sys/system_properties.h.
VARIANT_gles2_prop := $(GLES) -DHAVE_OPENGLES2 \
                      -DRUNLOOP_GLES_VERSION_PROPERTY \
                      -I$(ROOT)/tools/platform_stubs/android
VARIANT_gles32 := $(GLES) -DHAVE_OPENGLES3 -DHAVE_OPENGLES3_1 -DHAVE_OPENGLES3_2
VARIANT_gles3 := $(GLES) -DHAVE_OPENGLES3
VARIANTS := gles2_prop gles32 gles3
EXE :=

all: $(foreach v,$(VARIANTS),$(TARGET)_$(v)$(EXE))

$(TARGET)_%$(EXE): $(SOURCES) $(ROOT)/runloop.c $(ROOT)/runloop.h
	$(CC) $(CFLAGS) $(VARIANT_$*) $(SOURCES) -o $@ $(LDFLAGS)

check: all
	@for v in $(VARIANTS); do echo "== $$v"; ./$(TARGET)_$$v$(EXE) || exit 1; done

clean:
	rm -f $(foreach v,$(VARIANTS),$(TARGET)_$(v) $(TARGET)_$(v).exe)

.PHONY: all check clean
