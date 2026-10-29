# gfx/drivers/gxm_gfx.c's upload of the core's frame into its texture,
# on the host against the tree's psp2 stubs (tools/platform_stubs/vita).
# The test includes the driver's source, its tables made static so the
# compiler drops all of it the upload does not reach. Builds for Linux
# and, with CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET    := gxm_frame_upload_test$(EXE)
REPO_ROOT := ../../..

CFLAGS  += -Wall -Wno-unused-function -Wno-unused-variable -std=gnu99 -g1 -O1 \
           -DRARCH_INTERNAL -DHAVE_MENU -DVITA -DRARCH_CONSOLE \
           -U__linux__ -U__gnu_linux__ -Ulinux -U__unix__ -U__unix -Uunix \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
           -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/tools/platform_stubs/vita \
           -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): gxm_frame_upload_test.c $(REPO_ROOT)/gfx/drivers/gxm_gfx.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f gxm_frame_upload_test gxm_frame_upload_test.exe

.PHONY: all check clean
