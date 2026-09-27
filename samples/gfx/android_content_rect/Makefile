# Android's content rectangle, from the UI thread to the video thread.
#
# The two ends are the shipping functions, taken out of their files at
# build time by extract.awk: onContentRectChanged() from
# frontend/drivers/platform_unix.c and the Vulkan context's
# check_window() from gfx/drivers_context/android_vk_ctx.c. The test
# supplies the few types they touch, so it builds and runs on the host.
TARGET := android_content_rect_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I.
LDFLAGS +=

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

content_rect_driver.h: extract.awk $(CORE_DIR)/frontend/drivers/platform_unix.c \
		$(CORE_DIR)/gfx/drivers_context/android_vk_ctx.c
	( awk -v FUNCS=onContentRectChanged -f extract.awk \
	     $(CORE_DIR)/frontend/drivers/platform_unix.c && \
	  awk -v FUNCS=android_gfx_ctx_vk_check_window -f extract.awk \
	     $(CORE_DIR)/gfx/drivers_context/android_vk_ctx.c ) > $@.tmp
	mv $@.tmp $@

$(TARGET): android_content_rect_test.c content_rect_driver.h
	$(CC) $(CFLAGS) -o $@ android_content_rect_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) content_rect_driver.h content_rect_driver.h.tmp

.PHONY: all check clean
