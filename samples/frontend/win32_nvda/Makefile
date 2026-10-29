# frontend/drivers/nvda_controller_win32.c against a stand-in NVDA, an
# RPC server in the test, built and run for x64 and x86 both.
#
#   make check
#       with the mingw-w64 cross compilers, run under wine (WINE64 and
#       WINE32 name a wine for each, with its WINEPREFIX); skipped
#       where those are missing
#   make check CC=gcc [NVDA_DLL=...]
#       from an MSYS2 MINGW64 or MINGW32 shell: that one architecture
#
#   make check NVDA_DLL64=x64/nvdaControllerClient.dll \
#              NVDA_DLL32=x86/nvdaControllerClient.dll
#       the same calls first through NVDA's own controller client (from
#       its controllerClient zip), to hold the stand-in to it
TARGET64 := win32_nvda_test_x64.exe
TARGET32 := win32_nvda_test_x86.exe

RARCH_DIR := ../../..
SRC       := win32_nvda_test.c
DEPS      := $(SRC) $(RARCH_DIR)/frontend/drivers/nvda_controller_win32.c \
             $(RARCH_DIR)/frontend/drivers/nvda_controller_win32.h

CFLAGS  += -std=gnu89 -Wall -Wextra -Wno-unused-parameter \
           -Wdeclaration-after-statement -g1 -O1 -DHAVE_NVDA \
           -I$(RARCH_DIR)/libretro-common/include
LDLIBS  += -lrpcrt4

CC64   ?= x86_64-w64-mingw32-gcc
CC32   ?= i686-w64-mingw32-gcc
WINE64 ?= wine
WINE32 ?= wine

NATIVE := $(findstring mingw,$(shell $(CC) -dumpmachine 2>/dev/null))

ifneq ($(NATIVE),)
all: win32_nvda_test.exe
win32_nvda_test.exe: $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
check: all
	./win32_nvda_test.exe $(NVDA_DLL)
else
HAVE64 := $(shell command -v $(CC64) >/dev/null 2>&1 && command -v $(firstword $(WINE64)) >/dev/null 2>&1 && echo yes)
HAVE32 := $(shell command -v $(CC32) >/dev/null 2>&1 && command -v $(firstword $(WINE32)) >/dev/null 2>&1 && echo yes)

all: $(if $(HAVE64),$(TARGET64)) $(if $(HAVE32),$(TARGET32))

$(TARGET64): $(DEPS)
	$(CC64) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)
$(TARGET32): $(DEPS)
	$(CC32) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

check: all
ifeq ($(HAVE64),yes)
	WINEDEBUG=-all $(WINE64) ./$(TARGET64) $(NVDA_DLL64)
else
	@echo "win32_nvda: skipped x64 - no $(CC64) or $(WINE64)"
endif
ifeq ($(HAVE32),yes)
	WINEDEBUG=-all $(WINE32) ./$(TARGET32) $(NVDA_DLL32)
else
	@echo "win32_nvda: skipped x86 - no $(CC32) or $(WINE32)"
endif
endif

clean:
	rm -f $(TARGET64) $(TARGET32) win32_nvda_test.exe

.PHONY: all check clean
