include ../../sample_objdir.mk

# encoding_crc32.c as the toolchains build it, on x86-64:
#   - GCC's assembly of it never names VPCLMULQDQ on ZMM, YMM or
#     XMM16-31, which an assembler before binutils 2.30 cannot encode
#   - the gas macro that spells the instruction out encodes every
#     register triple as the assembler and the SDM do (crc32_gas_test)
#   - it and encoding_rzstd.c share a unity build with a <cpuid.h>
#     that has no include guard
#
#   make && ./crc32_gas_test

TARGET := crc32_gas_test

LIBRETRO_COMM_DIR := ../../..

# features_cpu.c sleeps through rtime.c on Windows
SOURCES := \
	crc32_gas_test.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

PREDEF := $(shell $(CC) -dM -E -x c /dev/null 2>/dev/null)
X86_64 := $(if $(filter __x86_64__,$(PREDEF)),1)
CLANG  := $(if $(filter __clang__,$(PREDEF)),1)

# The assembler's own vpclmulqdq on ZMM, where it has one
NATIVE := $(shell printf 'vpclmulqdq $$0x11,%%zmm1,%%zmm2,%%zmm3\n' \
	| $(CC) -c -x assembler -o /dev/null - 2>/dev/null && echo 1)
ifeq ($(NATIVE),1)
   CFLAGS += -DCRC32_GAS_NATIVE
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CHECKS :=
ifeq ($(X86_64),1)
   CHECKS += $(SAMPLE_OBJDIR)/unity.ok
ifneq ($(CLANG),1)
   CHECKS += $(SAMPLE_OBJDIR)/asm.ok
endif
endif

all: $(CHECKS) $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Every optimisation level, since inlining decides which registers the
# 128-bit tails get
$(SAMPLE_OBJDIR)/asm.ok: $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c
	@mkdir -p $(@D)
	@set -e; for o in -O1 -O2 -O3 -Os; do \
	   $(CC) $(CFLAGS) $$o -S -o $(SAMPLE_OBJDIR)/crc32$$o.s $<; \
	   if grep -nE '^[[:space:]]*vpclmulqdq[[:space:]].*%(zmm|ymm|xmm(1[6-9]|2[0-9]|3[01]))' \
	         $(SAMPLE_OBJDIR)/crc32$$o.s; then \
	      echo "FAIL ($$o): VPCLMULQDQ in a form binutils before 2.30 cannot encode"; \
	      exit 1; \
	   fi; \
	done
	@echo "[pass] encoding_crc32.c assembles without VPCLMULQDQ support"
	@touch $@

# The compiler's <cpuid.h> with its guard taken out, checked to break a
# double include before the unity build is held to surviving one
$(SAMPLE_OBJDIR)/unity.ok: cpuid_unity.c \
		$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
		$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c
	@mkdir -p $(SAMPLE_OBJDIR)/noguard
	@set -e; \
	h=$$(echo '#include <cpuid.h>' | $(CC) -E -x c - \
	   | sed -n 's/^# [0-9]* "\(.*cpuid\.h\)".*/\1/p' | head -n 1); \
	sed -e 's/^#ifndef _CPUID_H_INCLUDED$$/#if 1/' \
	    -e 's/^#define _CPUID_H_INCLUDED$$//' \
	    -e 's/^#ifndef __CPUID_H$$/#if 1/' \
	    -e 's/^#define __CPUID_H$$//' \
	    "$$h" > $(SAMPLE_OBJDIR)/noguard/cpuid.h; \
	printf '#include <cpuid.h>\n#include <cpuid.h>\n' \
	   > $(SAMPLE_OBJDIR)/noguard/twice.c; \
	if $(CC) -fsyntax-only -I$(SAMPLE_OBJDIR)/noguard \
	      $(SAMPLE_OBJDIR)/noguard/twice.c 2>/dev/null; then \
	   echo "FAIL: could not take the include guard out of $$h"; \
	   exit 1; \
	fi
	$(CC) -fsyntax-only -std=gnu99 -DHAVE_RZSTD \
	   -I$(SAMPLE_OBJDIR)/noguard -I$(LIBRETRO_COMM_DIR)/include cpuid_unity.c
	@echo "[pass] the unity build survives an unguarded <cpuid.h>"
	@touch $@

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
