include ../../sample_objdir.mk

TARGET := image_hdr_blit_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := image_hdr_blit_test.c \
	$(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) image_hdr_blit_thread_test

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJS): $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_tone_lut.h $(LIBRETRO_COMM_DIR)/include/formats/rwebm_video.h

check: all check-threaded
	./$(TARGET)

image_hdr_blit_thread_test: image_hdr_blit_thread_test.c $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_tone_lut.h $(LIBRETRO_COMM_DIR)/include/formats/rwebm_video.h
	$(CC) $(CFLAGS) -DHAVE_THREADS -o $@ image_hdr_blit_thread_test.c $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c $(LDFLAGS) $(if $(filter Windows_NT,$(OS)),-lwinmm,-pthread)

check-threaded: image_hdr_blit_thread_test
	./image_hdr_blit_thread_test

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)
	rm -f image_hdr_blit_thread_test image_hdr_blit_thread_test.exe

.PHONY: all check check-threaded clean
