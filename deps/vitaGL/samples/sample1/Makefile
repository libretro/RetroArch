SAMPLE_NUM	:= 001
TARGET		:= vitaGL-Sample$(SAMPLE_NUM)
SOURCES		:= .
			
INCLUDES	:= include

LIBS = -lvitaGL -lSceLibKernel_stub -lScePvf_stub -lmathneon \
	-lSceAppMgr_stub -lSceAppUtil_stub -lScePgf_stub \
	-ljpeg -lfreetype -lc -lSceCommonDialog_stub -lpng16 -lm -lz \
	-lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub \

CFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.c))
CPPFILES   := $(foreach dir,$(SOURCES), $(wildcard $(dir)/*.cpp))
BINFILES := $(foreach dir,$(DATA), $(wildcard $(dir)/*.bin))
OBJS     := $(addsuffix .o,$(BINFILES)) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) 

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CXX      = $(PREFIX)-g++
CFLAGS  = -g -Wl,-q -O2 -ftree-vectorize
CXXFLAGS  = $(CFLAGS) -fno-exceptions -std=gnu++11 -fpermissive
ASFLAGS = $(CFLAGS)

all: $(TARGET).vpk

$(TARGET).vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=VITAGL$(SAMPLE_NUM) "$(TARGET)" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin -a texture.bmp=texture.bmp $@

eboot.bin: $(TARGET).velf
	vita-make-fself -s $< eboot.bin	
	
%.velf: %.elf
	vita-elf-create $< $@
	
$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@
	
clean:
	@rm -rf *.velf *.elf *.vpk $(OBJS) param.sfo eboot.bin
