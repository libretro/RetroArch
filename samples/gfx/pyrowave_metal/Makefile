CC ?= cc
RA := ../../..
COMMON := $(RA)/libretro-common
PYTHON ?= python3
ASSETDIR ?= .metal-shaders
TARGET := pyrowave_metal_test$(EXE)
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -I$(COMMON)/include
# Match the shared boolean.h ABI: Clang exposes bool as _Bool in C89.
# Keep C89 and the remaining pedantic diagnostics enabled.
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
SRC := pyrowave_metal_test.c $(RA)/record/record_pyrowave_metal.c \
       $(RA)/record/record_pyrowave_metal_runtime.c $(RA)/record/record_gpu_queue.c \
       $(COMMON)/queues/retro_spsc.c $(COMMON)/formats/pyrowave/rpyrowave.c \
       $(COMMON)/formats/pyrowave/rpyrowave_gpu.c
NATIVE_C_SOURCES := $(filter-out pyrowave_metal_test.c,$(SRC)) $(COMMON)/formats/pyrowave/rpyrowave_image.c
NATIVE_OBJECTS := $(foreach source,$(NATIVE_C_SOURCES),.native/$(notdir $(source:.c=.o)))
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

native-compile:
	$(CC) $(CFLAGS) -DRECORD_PYROWAVE_METAL_FORCE_NATIVE -c $(RA)/record/record_pyrowave_metal_runtime.c -o metal_runtime_test.o

define METAL_NATIVE_OBJECT
.native/$(notdir $(1:.c=.o)): $(1)
	@mkdir -p .native
	$$(CC) $$(CFLAGS) -c $$< -o $$@
endef
$(foreach source,$(NATIVE_C_SOURCES),$(eval $(call METAL_NATIVE_OBJECT,$(source))))

$(ASSETDIR)/rpyrowave_shaders.h: $(RA)/tools/pyrowave_shaders.py $(RA)/tools/spirv_optimize.py $(RA)/tools/glsl_minify.py \
      $(COMMON)/formats/pyrowave/shaders/color.hlsl \
      $(COMMON)/formats/pyrowave/shaders/dwt.hlsl \
      $(COMMON)/formats/pyrowave/shaders/packet.hlsl
	$(PYTHON) $(RA)/tools/pyrowave_shaders.py --output $@ --export-msl $(ASSETDIR) --metal-sdk macosx

.native/native_gpu_test.o: native_gpu_test.m $(ASSETDIR)/rpyrowave_shaders.h
	@mkdir -p .native
	$(CC) -O2 -g -std=gnu89 -Wall -Wextra -Werror -Wno-deprecated-declarations \
          -fobjc-arc -fblocks -mmacosx-version-min=10.14 -I$(COMMON)/include -I$(ASSETDIR) -c $< -o $@

pyrowave_metal_native_test: $(NATIVE_OBJECTS) .native/native_gpu_test.o
	$(CC) $^ -framework Foundation -framework Metal -lobjc -lm -o $@

native: pyrowave_metal_native_test

native-check: native
	./pyrowave_metal_native_test

clean:
	rm -f pyrowave_metal_test pyrowave_metal_test.exe metal_runtime_test.o
	rm -f pyrowave_metal_native_test
	rm -rf .native .metal-shaders

.PHONY: all check native-compile native native-check clean
