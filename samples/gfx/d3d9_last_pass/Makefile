# The preset setup of gfx/drivers/d3d9hlsl.c and gfx/drivers/d3d9cg.c:
# each test includes its driver source and d3d9_last_pass_cases.h, which
# holds the scripted preset loader and the cases the two drivers must
# decide alike; the rest of the frontend is stubbed in stubs.c.  Direct3D
# 9 is Windows-only, so this builds with mingw-w64 (cross, or MSYS2 on
# Windows) and runs there or under Wine.

include ../../sample_objdir.mk

ifeq ($(OS),Windows_NT)
CROSS ?=
else
CROSS ?= x86_64-w64-mingw32-
endif
CC := $(CROSS)gcc

TARGETS := d3d9hlsl_last_pass_test.exe d3d9cg_last_pass_test.exe

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps -I$(CORE_DIR)/gfx/include
CFLAGS  += -Wall -Wno-unused-function $(INCDIRS) -std=gnu99 -g1 -O1 \
	-DRARCH_INTERNAL -DHAVE_D3D -DHAVE_D3D9 -DHAVE_HLSL -DHAVE_CG \
	-DHAVE_THREADS -DHAVE_MENU
LDFLAGS += -static

COMPAT_OBJ := $(call sample_objs,$(LIBRETRO_COMM_DIR)/compat/compat_strl.o)

all: $(TARGETS)

d3d9hlsl_last_pass_test.exe: d3d9hlsl_last_pass_test.o stubs_hlsl.o $(COMPAT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

d3d9cg_last_pass_test.exe: d3d9cg_last_pass_test.o stubs_cg.o $(COMPAT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

d3d9hlsl_last_pass_test.o: $(CORE_DIR)/gfx/drivers/d3d9hlsl.c d3d9_last_pass_cases.h
d3d9cg_last_pass_test.o: $(CORE_DIR)/gfx/drivers/d3d9cg.c d3d9_last_pass_cases.h

stubs_hlsl.o: stubs.c
	$(CC) -c -o $@ $<

stubs_cg.o: stubs.c
	$(CC) -DD3D9_LAST_PASS_CG -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(OS),Windows_NT)
RUN :=
else
RUN := WINEDEBUG=-all wine
endif

check: $(TARGETS)
	@for t in $(TARGETS); do echo "$$t:"; $(RUN) ./$$t || exit 1; done

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f *.o $(TARGETS)

.PHONY: all check clean
