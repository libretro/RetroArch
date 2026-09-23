# gfx/common/rgba16_pack.h: the vector path against the scalar one.
#
# Two binaries: the pack as this target compiles it (SSE2 on x86, NEON
# on ARM) and the scalar path forced with RGBA16_PACK_NO_SIMD. Both are
# held to the same reference. The NEON path on an x86 host:
#    make CC=arm-linux-gnueabihf-gcc EXTRA_CFLAGS="-mfpu=neon -static"
#    qemu-arm-static ./rgba16_pack_test
TARGET := rgba16_pack_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

# The reference is c * 65535 + 0.5 as two roundings; a contracted
# multiply-add would move the boundaries it is checking.
CFLAGS  += -Wall -std=c89 -pedantic -Wno-long-long \
           -Wdeclaration-after-statement -ffp-contract=off -g -O2 \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) $(EXTRA_CFLAGS)
LDFLAGS += $(EXTRA_CFLAGS)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET)_scalar

$(TARGET): rgba16_pack_test.c $(CORE_DIR)/gfx/common/rgba16_pack.h
	$(CC) $(CFLAGS) -o $@ rgba16_pack_test.c $(LDFLAGS) -lm

$(TARGET)_scalar: rgba16_pack_test.c $(CORE_DIR)/gfx/common/rgba16_pack.h
	$(CC) $(CFLAGS) -DRGBA16_PACK_NO_SIMD -o $@ rgba16_pack_test.c $(LDFLAGS) -lm

check: all
	./$(TARGET)
	./$(TARGET)_scalar

clean:
	rm -f $(TARGET) $(TARGET)_scalar

.PHONY: all check clean
