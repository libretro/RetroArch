TARGET          := libmathneon
SOURCES         := source

LIBS = -lc -lm -lSceGxm_stub -lSceDisplay_stub

CFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.c))
CGFILES  := $(foreach dir,$(SHADERS), $(wildcard $(dir)/*.cg))
HEADERS  := $(CGFILES:.cg=.h)
OBJS     := $(CFILES:.c=.o)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
AR      = $(PREFIX)-gcc-ar
CFLAGS  = -g -Wl,-q -O2 -ffast-math -mtune=cortex-a9 -mfpu=neon -flto -ftree-vectorize
ASFLAGS = $(CFLAGS)

all: $(TARGET).a

$(TARGET).a: $(OBJS)
	$(AR) -rc $@ $^
	
clean:
	@rm -rf $(TARGET).a $(TARGET).elf $(OBJS)
	
install: $(TARGET).a
	@mkdir -p $(VITASDK)/$(PREFIX)/lib/
	cp $(TARGET).a $(VITASDK)/$(PREFIX)/lib/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/
	cp source/math_neon.h $(VITASDK)/$(PREFIX)/include/
