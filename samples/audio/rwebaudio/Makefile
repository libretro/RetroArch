include ../../sample_objdir.mk

TARGET := rwebaudio_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The driver is #included by the test and built against the tree's
# compile-only emscripten stand-ins; its JavaScript half is a stand-in
# in the test, and runs for real under Node in `make check`.
INCDIRS := -I../../../tools/platform_stubs/emscripten -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

# Built as the published web player is: the external block, faked by
# going back to the browser's main loop.
CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE \
           -DEMSCRIPTEN_AUDIO_EXTERNAL_BLOCK -DEMSCRIPTEN_AUDIO_FAKE_BLOCK
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,rwebaudio_test.o rwebaudio_asyncify_case.o ../../../audio/audio_upmix.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

rwebaudio_test.o: rwebaudio_test.c ../../../audio/drivers/rwebaudio.c
	$(CC) $(CFLAGS) -c -o $@ $<

rwebaudio_asyncify_case.o: rwebaudio_asyncify_case.c ../../../audio/drivers/rwebaudio.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)
	node rwebaudio_js_test.js

# Compile with the real SDK: a Node mock alone cannot validate JS-library
# decorators or the wasm import's Asyncify suspension/return convention.
EMCC ?= emcc
NODE ?= node
emscripten-check:
	@mkdir -p $(SAMPLE_OBJDIR)
	$(EMCC) -O1 -sASYNCIFY=1 -sENVIRONMENT=node -sEXIT_RUNTIME=1 -sWASM_ASYNC_COMPILATION=0 \
		--js-library ../../../os/emscripten/library_rwebaudio.js \
		rwebaudio_emscripten_test.c -o $(SAMPLE_OBJDIR)/rwebaudio-wasm.js
	$(NODE) $(SAMPLE_OBJDIR)/rwebaudio-wasm.js

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean emscripten-check
