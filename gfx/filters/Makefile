compiler    := gcc
extra_flags :=
use_neon    := 0
release		:= release

ifndef platform
platform := $(shell $(compiler) -dumpmachine)
endif

ifneq (,$(findstring armv7,$(platform)))
extra_flags += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
use_neon := 1
endif

ifneq (,$(findstring hardfloat,$(platform)))
extra_flags += -mfloat-abi=hard
endif

ifeq (release,$(build))
extra_flags += -O2
endif

ifeq (debug,$(build))
extra_flags += -O0 -g
endif

cc      := $(compiler)
cpp     := $(subst cc,++,$(compiler)) -std=gnu++0x
flags   := -fPIC $(extra_flags)
asflags := -fPIC  $(extra_flags)
ldflags := -shared -Wl,--version-script=link.T
objects :=
flags   += -std=c99


ifeq (1,$(use_neon))
ASMFLAGS := -INEON/asm
asflags += -mfpu=neon
endif
           
objects += 2xsai.so super2xsai.so supereagle.so 2xbr.so darken.so hq2x.so scale2x.so

all: build;

%.o: %.S
	$(cc) -c -o $@ $(asflags)  $(ASMFLAGS)  $<

%.o: %.c
	$(cc) -c -o $@ $(flags) $<

%.so: %.o
	$(cc) -o $@ $(ldflags) $(flags) $^

build: $(objects)

clean:
	rm -f *.o
	rm -f *.so

strip:
	strip -s *.so
