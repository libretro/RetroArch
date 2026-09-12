# The WDM-KS ABI, checked against the system's own ks.h and ksmedia.h.
#
# Nothing runs here: the assertions in wdmks_abi_check.c are the test,
# and a mismatch is a build failure. Cross-compiled, because the
# headers being checked against are Windows ones - i686 as well as
# x86-64, since the structures are the same size on both only because
# none of them holds a pointer, and that is worth proving rather than
# asserting.

CORE_DIR := ../../..
CC64     ?= x86_64-w64-mingw32-gcc
CC32     ?= i686-w64-mingw32-gcc
# -Wno-unused-const-variable: this compiles the whole driver, and it
# declares KS constants the ABI check compares but the current stage
# does not yet call - the capture category and the WAVEFORMATEX
# specifier. Checking them is the point; using them comes later.
CFLAGS   += -Wall -Wextra -Wno-unused-const-variable -Wno-unused-function \
            -DRARCH_INTERNAL -DHAVE_THREADS \
            -I$(CORE_DIR) -I$(CORE_DIR)/libretro-common/include

all: check

check:
	@$(CC64) $(CFLAGS) -std=gnu99 -c -o /dev/null wdmks_abi_check.c
	@echo "  x86-64 gnu99: sizes, offsets and constants match ks.h"
	@$(CC64) $(CFLAGS) -std=c89 -pedantic -Wno-long-long -c -o /dev/null wdmks_abi_check.c
	@echo "  x86-64 c89:   the header is C89, which the 2005 toolchain needs"
	@if command -v $(CC32) >/dev/null 2>&1; then \
	   $(CC32) $(CFLAGS) -std=gnu99 -c -o /dev/null wdmks_abi_check.c && \
	   echo "  i686   gnu99: same sizes on 32-bit, as they must be"; \
	 else echo "  i686: no $(CC32), skipped"; fi
	@python3 $(CORE_DIR)/tools/wdmks_abi_guids.py

clean:
	@rm -f *.o

.PHONY: all check clean
