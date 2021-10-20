compiler    := gcc
extra_flags :=
use_neon    := 0
build       = release
DYLIB	      := so
PREFIX      := /usr
INSTALLDIR  := $(PREFIX)/lib/retroarch/filters/audio

ifeq ($(platform),)
   platform = unix
   ifeq ($(shell uname -s),)
      platform = win
   else ifneq ($(findstring Darwin,$(shell uname -s)),)
      platform = osx
      arch     = intel
      ifeq ($(shell uname -p),powerpc)
         arch = ppc
      endif
   else ifneq ($(findstring MINGW,$(shell uname -s)),)
      platform = win
   endif
endif

ifeq ($(platform),gcc)
   extra_rules_gcc := $(shell $(compiler) -dumpmachine)
endif

ifneq (,$(findstring armv7,$(extra_rules_gcc)))
   extra_flags += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
   use_neon := 1
endif

ifneq (,$(findstring hardfloat,$(extra_rules_gcc)))
   extra_flags += -mfloat-abi=hard
endif

ifeq (release,$(build))
   extra_flags += -O2
endif

ifeq (debug,$(build))
   extra_flags += -O0 -g
endif

ldflags := $(LDFLAGS) -shared -lm -Wl,--version-script=link.T

ifeq ($(platform), unix)
   DYLIB = so
else ifeq ($(platform), osx)
   compiler := $(CC)
   DYLIB = dylib
   ldflags := -dynamiclib
   ARCHFLAGS=
   MINVERFLAGS=
   ifeq ($(shell uname -p),arm)
      MINVERFLAGS = -mmacosx-version-min=10.15 -stdlib=libc++ # macOS  (Metal, ARM 64bit)
   else ifeq ($(HAVE_METAL),1)
      MINVERFLAGS = -mmacosx-version-min=10.13 -stdlib=libc++  # macOS  (Metal, x86 64bit)
   else ifeq ($(shell uname -p),powerpc)
      MINVERFLAGS = -mmacosx-version-min=10.5  # macOSX (PowerPC 32-bit)
   else ifeq ($(shell uname -m),i386)
      MINVERFLAGS = -mmacosx-version-min=10.6  # macOSX (OpenGL, x86 32bit)
   else
      MINVERFLAGS = -mmacosx-version-min=10.7 -stdlib=libc++ # macOSX (OpenGL, x86 64bit)
   endif

	# Build for a specific architecture when ARCH is defined as a switch
   ifeq ($(ARCH),arm64)
      MINVERFLAGS  = -mmacosx-version-min=10.15 -stdlib=libc++ # macOS  (Metal, ARM 64bit)
      ARCHFLAGS    = -arch arm64
   else ifeq ($(ARCH),x86_64)
      ifeq ($(HAVE_METAL),1)
         MINVERFLAGS  = -mmacosx-version-min=10.13 -stdlib=libc++
      else
         MINVERFLAGS  = -mmacosx-version-min=10.7  -stdlib=libc++
      endif
      ARCHFLAGS       = -arch x86_64
   else ifeq ($(ARCH),x86)
      MINVERFLAGS     = -mmacosx-version-min=10.6
      ARCHFLAGS       = -arch x86
   else ifeq ($(ARCH),ppc)
      MINVERFLAGS     = -mmacosx-version-min=10.5
      ARCHFLAGS       = -arch ppc
   endif
   ifeq ($(BUILDBOT),1)
      ARCHFLAGS       = -target $(LIBRETRO_APPLE_PLATFORM) -isysroot $(LIBRETRO_APPLE_ISYSROOT)
   endif
	extraflags += $(MINVERFLAGS) $(ARCHFLAGS)
	ldflags += $(MINVERFLAGS) $(ARCHFLAGS)
else
   extra_flags += -static-libgcc -static-libstdc++
   DYLIB = dll
endif

CC      := $(compiler) -Wall
CXX     := $(subst CC,++,$(compiler)) -std=gnu++0x -Wall
flags   := $(CPPFLAGS) $(CFLAGS) -fPIC $(extra_flags) -I../../include
asflags := $(ASFLAGS) -fPIC  $(extra_flags)
objects :=

ifeq (1,$(use_neon))
   ASMFLAGS := -INEON/asm
   asflags += -mfpu=neon
endif

plugs := $(wildcard *.c)
objects := $(plugs:.c=.o)
targets := $(objects:.o=.$(DYLIB))

all: build;

%.o: %.S
	$(CC) -c -o $@ $(asflags)  $(ASMFLAGS)  $<

%.o: %.c
	$(CC) -c -o $@ $(flags) $<

%.$(DYLIB): %.o
	$(CC) -o $@ $(ldflags) $(flags) $^

build: $(targets)

clean:
	rm -f *.o
	rm -f *.$(DYLIB)

strip:
	strip -s *.$(DYLIB)

install:
	mkdir -p $(DESTDIR)$(INSTALLDIR)
	cp -t $(DESTDIR)$(INSTALLDIR) $(targets) *.dsp

test-install:
	DESTDIR=/tmp/build $(MAKE) install
