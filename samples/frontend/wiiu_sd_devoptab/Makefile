# os/wiiu/fs/sd_fat_devoptab.c, the Wii U SD devoptab, against a stand-in
# Cafe FS that models DMA cache-line invalidation. stubs/ stands in for
# the part of devkitPPC's <sys/iosupport.h> the devoptab uses; the Wii U
# headers are the tree's own (os/wiiu/include). Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET   := wiiu_sd_devoptab_test$(EXE)
CORE_DIR := ../../..

CFLAGS  += -std=gnu89 -Wall -g1 -O1 -Istubs -I$(CORE_DIR)/os/wiiu/fs \
           -I$(CORE_DIR)/os/wiiu/include -Wno-unused-function \
           -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
           -Wno-address-of-packed-member

# Windows lacks <sys/statvfs.h> and struct stat's st_blocks.
ifeq ($(EXE),.exe)
   CFLAGS := -Istubs_win -Dst_blocks=st_rdev $(CFLAGS)
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): wiiu_sd_devoptab_test.c $(CORE_DIR)/os/wiiu/fs/sd_fat_devoptab.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f wiiu_sd_devoptab_test wiiu_sd_devoptab_test.exe

.PHONY: all check clean
