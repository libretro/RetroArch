TARGET          := libvitashark
SOURCES         := source
SHADERS         := shaders

CFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.c))
ASMFILES := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.S))
CGFILES  := $(foreach dir,$(SHADERS), $(wildcard $(dir)/*.cg))
HEADERS  := $(CGFILES:.cg=.h)
OBJS     := $(CFILES:.c=.o) $(ASMFILES:.S=.o)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
AR      = $(PREFIX)-gcc-ar
CFLAGS  = -g -Wl,-q -O2 -ffast-math -mtune=cortex-a9 -mfpu=neon -ftree-vectorize
ASFLAGS = $(CFLAGS)

all: $(TARGET).a

$(TARGET).a: $(OBJS)
	$(AR) -rc $@ $^
	
clean:
	@rm -rf $(TARGET).a $(TARGET).elf $(OBJS)
	@make -C samples/sample1 clean
	@make -C samples/sample2 clean
	
install: $(TARGET).a
	@mkdir -p $(VITASDK)/$(PREFIX)/lib/
	cp $(TARGET).a $(VITASDK)/$(PREFIX)/lib/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/
	cp source/vitashark.h $(VITASDK)/$(PREFIX)/include/
	
samples: $(TARGET).a
	@make -C samples/sample1
	cp "samples/sample1/vitaShaRK-Sample001.vpk" .
	@make -C samples/sample2
	cp "samples/sample1/vitaShaRK-Sample002.vpk" .
