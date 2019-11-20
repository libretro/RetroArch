TARGET          := libvitaGL
SOURCES         := source source/utils
SHADERS         := shaders

LIBS = -lc -lm -lSceGxm_stub -lSceDisplay_stub

ifeq ($(HAVE_SBRK),1)
SOURCES += source/hacks
endif

CFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.c))
CGFILES  := $(foreach dir,$(SHADERS), $(wildcard $(dir)/*.cg))
HEADERS  := $(CGFILES:.cg=.h)
OBJS     := $(CFILES:.c=.o)

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
AR      = $(PREFIX)-gcc-ar
CFLAGS  = -g -Wl,-q -O2 -ffast-math -mtune=cortex-a9 -mfpu=neon -flto -ftree-vectorize -DTRANSPOSE_MATRICES
ASFLAGS = $(CFLAGS)

all: $(TARGET).a

$(TARGET).a: $(OBJS)
	$(AR) -rc $@ $^

%_f.h:
	psp2cgc -profile sce_fp_psp2 $(@:_f.h=_f.cg) -Wperf -fastprecision -O3 -o $(@:_f.h=_f.gxp)
	bin2c $(@:_f.h=_f.gxp) source/shaders/$(notdir $(@)) $(notdir $(@:_f.h=_f))
	@rm -rf $(@:_f.h=_f.gxp)
	
%_v.h:
	psp2cgc -profile sce_vp_psp2 $(@:_v.h=_v.cg) -Wperf -fastprecision -O3 -o $(@:_v.h=_v.gxp)
	bin2c $(@:_v.h=_v.gxp) source/shaders/$(notdir $(@:_v.h=_v.h)) $(notdir $(@:_v.h=_v))
	@rm -rf $(@:_v.h=_v.gxp)

shaders: $(HEADERS)
	
clean:
	@rm -rf $(TARGET).a $(TARGET).elf $(OBJS)
	@make -C samples/sample1 clean
	@make -C samples/sample2 clean
	@make -C samples/sample3 clean
	@make -C samples/sample4 clean
	@make -C samples/sample5 clean
	@make -C samples/sample6 clean
	@make -C samples/sample7 clean
	
install: $(TARGET).a
	@mkdir -p $(VITASDK)/$(PREFIX)/lib/
	cp $(TARGET).a $(VITASDK)/$(PREFIX)/lib/
	@mkdir -p $(VITASDK)/$(PREFIX)/include/
	cp source/vitaGL.h $(VITASDK)/$(PREFIX)/include/
	
samples: $(TARGET).a
	@make -C samples/sample1
	cp "samples/sample1/vitaGL-Sample001.vpk" .
	@make -C samples/sample2
	cp "samples/sample2/vitaGL-Sample002.vpk" .
	@make -C samples/sample3
	cp "samples/sample3/vitaGL-Sample003.vpk" .
	@make -C samples/sample4
	cp "samples/sample4/vitaGL-Sample004.vpk" .
	@make -C samples/sample5
	cp "samples/sample5/vitaGL-Sample005.vpk" .
	@make -C samples/sample6
	cp "samples/sample6/vitaGL-Sample006.vpk" .
	@make -C samples/sample7
	cp "samples/sample7/vitaGL-Sample007.vpk" .
