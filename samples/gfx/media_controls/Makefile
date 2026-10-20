CC ?= cc
THREADS ?= 1
TARGET := media_controls_test
CFLAGS += -O2 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wno-unused-parameter -Wframe-larger-than=4096 -D_GNU_SOURCE -I../../.. -I../../../libretro-common/include
SOURCES := media_controls_test.c ../../../gfx/gfx_media_controls.c ../../../menu/menu_media_controls.c
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
SOURCES += ../../../libretro-common/rthreads/rthreads.c
LDFLAGS += -lpthread
endif
ifeq ($(OS),Windows_NT)
TARGET := $(TARGET).exe
LDFLAGS += -lwinmm
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES) ../../../gfx/gfx_media_controls.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f media_controls_test media_controls_test.exe
.PHONY: all check clean
