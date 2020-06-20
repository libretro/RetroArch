TITLE_ID = RAXXXXXXX
TARGET   = Launcher
OBJS     = main.o

LIBS     = -lSceAppMgr_stub

PREFIX   = arm-vita-eabi
CC       = $(PREFIX)-gcc
CFLAGS   = -Wl,-q -Wall
ASFLAGS  = $(CFLAGS)

all: builder/eboot.bin

%.vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "Launcher" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin $@ \
		-a contents/icon0.png=sce_sys/icon0.png \
		-a contents/bg.png=sce_sys/livearea/contents/bg.png \
		-a contents/startup.png=sce_sys/livearea/contents/startup.png \
		-a contents/template.xml=sce_sys/livearea/contents/template.xml \
		-a contents/args.txt=args.txt

builder/eboot.bin: $(TARGET).velf
	vita-make-fself -s $< $@

%.velf: %.elf
	vita-elf-create $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

clean:
	@rm -rf $(TARGET).vpk $(TARGET).velf $(TARGET).elf $(OBJS) \
		eboot.bin param.sfo
