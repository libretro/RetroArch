TARGET   := psp1_gfx_contracts_test

ROOT     := ../../..
STUBS    := $(ROOT)/tools/platform_stubs/psp

# fake_psp.c compiles gfx/drivers/psp1_gfx.c as it ships, against the
# same PSP stub headers the compile matrix builds it with.
SOURCES  := \
	psp1_gfx_contracts_test.c \
	fake_psp.c

OBJS     := $(SOURCES:.c=.o)
DEPS     := $(OBJS:.o=.d)

# fake_psp.c includes a file from the tree, so track that dependency.
CFLAGS  += -Wall -std=gnu99 -g -O1 -MMD -MP \
           -I$(STUBS) -I$(ROOT) -I$(ROOT)/libretro-common/include \
           -I$(ROOT)/deps -I$(ROOT)/deps/rcheevos/include \
           -I$(ROOT)/input/include \
           -DRARCH_INTERNAL -DHAVE_AUDIOMIXER -DHAVE_THREADS \
           -DHAVE_CONFIGFILE -DHAVE_MENU -DPSP \
           -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
LDFLAGS += -lm

# Slice grid to build against, e.g.
#   make GRID="-DPSP_FRAME_ROWS_COUNT=16 -DPSP_FRAME_COLUMNS_COUNT=4"
# Both TUs take it, so the coordinate contracts check that split.
CFLAGS  += $(GRID)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

# The splits worth timing on hardware, as "rows columns". The grid's
# shape is what the GE's texture cache sees; what it costs can only be
# measured on the device, so this only settles that each one still
# tiles the viewport and spans the frame, leaving speed the difference.
GRIDS := "4 16" "16 4" "8 8" "16 1" "32 1"

check-grids:
	@rc=0; for g in $(GRIDS); do \
	  set -- $$g; \
	  printf '%-8s ' "$$1x$$2"; \
	  $(MAKE) -s clean >/dev/null 2>&1; \
	  if $(MAKE) -s GRID="-DPSP_FRAME_ROWS_COUNT=$$1 -DPSP_FRAME_COLUMNS_COUNT=$$2" \
	        >/dev/null 2>&1 && ./$(TARGET) >/dev/null 2>&1; then \
	    echo "ok"; \
	  else \
	    echo "FAILED"; rc=1; \
	  fi; \
	done; $(MAKE) -s clean >/dev/null 2>&1; exit $$rc

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all check check-grids clean
