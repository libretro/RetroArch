# The second display's surface, from the Android UI thread to the
# renderer: nativeSurfaceChanged(), nativeSurfaceDestroyed() and
# android_second_surface_term() from frontend/drivers/platform_unix.c,
# android_windows_poll() and the keep across a driver reinit from
# gfx/common/android_windows.c, the GL context's poll from
# gfx/drivers_context/android_ctx.c and Vulkan's kind of buffers from
# gfx/drivers/vulkan.c, taken out of their files at build time by
# extract.awk.
include ../../sample_objdir.mk

TARGET   := android_second_surface_test$(EXE)
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common
JNI      := Java_com_retroarch_browser_retroactivity_SecondDisplayController

SOURCES := android_second_surface_test.c \
           $(CORE_DIR)/gfx/video_windows.c \
           $(CORE_DIR)/gfx/video_views.c
OBJS    := $(call sample_objs,$(SOURCES:.c=.o))
CFLAGS  += -Wall -std=c89 -pedantic -Werror=declaration-after-statement \
           -Wno-long-long -Wno-variadic-macros -g1 -O1 -I$(LRC)/include -I$(CORE_DIR) -I.
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

second_surface_driver.h: extract.awk $(CORE_DIR)/frontend/drivers/platform_unix.c \
		$(CORE_DIR)/gfx/common/android_windows.c \
		$(CORE_DIR)/gfx/drivers_context/android_ctx.c \
		$(CORE_DIR)/gfx/drivers/vulkan.c
	( awk -v FUNCS=android_second_surface_free,android_second_surface_term,$(JNI)_nativeSurfaceChanged,$(JNI)_nativeSurfaceDestroyed \
	     -f extract.awk $(CORE_DIR)/frontend/drivers/platform_unix.c && \
	  awk -v FUNCS=android_windows_at,android_windows_unbind,android_windows_poll,android_windows_let_go,android_windows_destroy,android_windows_keep,android_windows_take,android_windows_release_kept,android_windows_find,android_windows_free \
	     -f extract.awk $(CORE_DIR)/gfx/common/android_windows.c && \
	  awk -v FUNCS=android_gfx_ctx_window_buffers,android_gfx_ctx_window_poll \
	     -f extract.awk $(CORE_DIR)/gfx/drivers_context/android_ctx.c && \
	  awk -v FUNCS=vulkan_window_buffers \
	     -f extract.awk $(CORE_DIR)/gfx/drivers/vulkan.c ) > $@.tmp
	mv $@.tmp $@

android_second_surface_test.o: android_second_surface_test.c second_surface_driver.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	@sample_objdir="$(SAMPLE_OBJDIR)"; rm -rf "$${sample_objdir:?}"
	rm -f android_second_surface_test android_second_surface_test.exe \
	      android_second_surface_test.o \
	      second_surface_driver.h second_surface_driver.h.tmp

.PHONY: all check clean
