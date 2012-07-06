include config.mk

TARGET = retroarch tools/retroarch-joyconfig

OBJ = retroarch.o \
		file.o \
		file_path.o \
		hash.o \
		driver.o \
		settings.o \
		dynamic.o \
		message.o \
		rewind.o \
		gfx/gfx_common.o \
		patch.o \
		compat/compat.o \
		screenshot.o \
		audio/null.o \
		input/null.o \
		gfx/null.o

JOYCONFIG_OBJ := tools/retroarch-joyconfig.o \
	conf/config_file.o \
	compat/compat.o

HEADERS = $(wildcard */*.h) $(wildcard *.h)

LIBS = -lm
DEFINES = -DHAVE_CONFIG_H -DHAVE_SCREENSHOTS

ifeq ($(REENTRANT_TEST), 1)
   DEFINES += -Dmain=retroarch_main
   OBJ += console/test.o
endif

ifneq ($(findstring Darwin,$(OS)),)
   OSX := 1
   LIBS += -framework AppKit
else
   OSX := 0
endif

BSD_LOCAL_INC =
DYLIB_LIB = -ldl
ifneq ($(findstring BSD,$(OS)),)
   BSD_LOCAL_INC = -I/usr/local/include
   DYLIB_LIB = -lc
endif

ifneq ($(findstring Linux,$(OS)),)
   LIBS += -lrt
   DEFINES += -DIS_LINUX
   OBJ += input/linuxraw_input.o
endif

ifeq ($(HAVE_THREADS), 1)
   OBJ += autosave.o thread.o
   LIBS += -lpthread
endif

ifeq ($(HAVE_CONFIGFILE), 1)
   OBJ += conf/config_file.o
endif

ifeq ($(HAVE_BSV_MOVIE), 1)
   OBJ += movie.o
endif

ifeq ($(HAVE_NETPLAY), 1)
   OBJ += netplay.o network_cmd.o
endif

ifeq ($(HAVE_RSOUND), 1)
   OBJ += audio/rsound.o
   LIBS += $(RSOUND_LIBS)
   DEFINES += $(RSOUND_CFLAGS)
endif

ifeq ($(HAVE_OSS), 1)
   OBJ += audio/oss.o
endif
ifeq ($(HAVE_OSS_BSD), 1)
   OBJ += audio/oss.o
endif
ifeq ($(HAVE_OSS_LIB), 1)
   LIBS += -lossaudio
endif

ifeq ($(HAVE_ALSA), 1)
   OBJ += audio/alsa.o
   LIBS += $(ALSA_LIBS)
   DEFINES += $(ALSA_CFLAGS)
endif
ifeq ($(HAVE_ROAR), 1)
   OBJ += audio/roar.o
   LIBS += $(ROAR_LIBS)
   DEFINES += $(ROAR_CFLAGS)
endif
ifeq ($(HAVE_AL), 1)
   OBJ += audio/openal.o
ifeq ($(OSX),1)
   LIBS += -framework OpenAL
else
   LIBS += -lopenal
endif
endif
ifeq ($(HAVE_JACK),1)
   OBJ += audio/jack.o
   LIBS += $(JACK_LIBS)
   DEFINES += $(JACK_CFLAGS)
endif
ifeq ($(HAVE_PULSE), 1)
   OBJ += audio/pulse.o
   LIBS += $(PULSE_LIBS)
   DEFINES += $(PULSE_CFLAGS)
endif

ifeq ($(HAVE_COREAUDIO), 1)
   OBJ += audio/coreaudio.o
   LIBS += -framework CoreServices -framework CoreAudio -framework AudioUnit
endif

ifeq ($(HAVE_SDL), 1)
   OBJ += gfx/sdl_gfx.o gfx/context/sdl_ctx.o input/sdl_input.o audio/sdl_audio.o fifo_buffer.o
   DEFINES += $(SDL_CFLAGS) $(BSD_LOCAL_INC)
   LIBS += $(SDL_LIBS)

ifeq ($(HAVE_OPENGL), 1)
	OBJ += gfx/gl.o gfx/fonts/freetype.o
ifeq ($(OSX),1)
	LIBS += -framework OpenGL
else
	LIBS += -lGL
endif
endif
endif

ifeq ($(HAVE_RPI), 1)
   OBJ += gfx/rpi.o
   LIBS += -lOpenVG -lGLESv2 -lEGL -lbcm_host
endif

ifeq ($(HAVE_XVIDEO), 1)
   OBJ += gfx/xvideo.o input/x11_input.o
   LIBS += $(XVIDEO_LIBS) $(X11_LIBS) $(XEXT_LIBS)
   DEFINES += $(XVIDEO_CFLAGS) $(X11_CFLAGS) $(XEXT_CFLAGS)
endif

ifeq ($(HAVE_CG), 1)
   OBJ += gfx/shader_cg.o
   LIBS += -lCg -lCgGL
endif

ifeq ($(HAVE_XML), 1)
   OBJ += cheats.o 
   LIBS += $(XML_LIBS)
   DEFINES += $(XML_CFLAGS)

ifeq ($(HAVE_OPENGL), 1)
   OBJ += gfx/shader_glsl.o 
endif
endif

ifeq ($(HAVE_XML), 1)
   OBJ += gfx/state_tracker.o gfx/image.o 
else ifeq ($(HAVE_CG), 1)
   OBJ += gfx/state_tracker.o gfx/image.o 
endif

ifeq ($(HAVE_DYLIB), 1)
   OBJ += gfx/ext_gfx.o audio/ext_audio.o
   LIBS += $(DYLIB_LIB)
endif

ifeq ($(HAVE_FREETYPE), 1)
   OBJ += gfx/fonts/fonts.o
   LIBS += $(FREETYPE_LIBS)
   DEFINES += $(FREETYPE_CFLAGS)
endif

ifeq ($(HAVE_SDL_IMAGE), 1)
   LIBS += $(SDL_IMAGE_LIBS)
   DEFINES += $(SDL_IMAGE_CFLAGS)
endif

ifeq ($(HAVE_LIBPNG), 1)
   LIBS += $(LIBPNG_LIBS)
   DEFINES += $(LIBPNG_CFLAGS)
endif

ifeq ($(HAVE_FFMPEG), 1)
   OBJ += record/ffemu.o
   LIBS += $(AVCODEC_LIBS) $(AVFORMAT_LIBS) $(AVUTIL_LIBS) $(SWSCALE_LIBS)
   DEFINES += $(AVCODEC_CFLAGS) $(AVFORMAT_CFLAGS) $(AVUTIL_CFLAGS) $(SWSCALE_CFLAGS)
endif

ifeq ($(HAVE_DYNAMIC), 1)
   LIBS += $(DYLIB_LIB)
else
   LIBS += $(libretro)
endif

ifeq ($(HAVE_PYTHON), 1)
   DEFINES += $(PYTHON_CFLAGS) -Wno-unused-parameter
   LIBS += $(PYTHON_LIBS)
   OBJ += gfx/py_state/py_state.o
endif

ifeq ($(HAVE_SINC), 1)
   OBJ += audio/sinc.o
else
   OBJ += audio/hermite.o
endif

ifneq ($(HAVE_FIXED_POINT), 1)
   OBJ += audio/utils.o
endif

ifneq ($(V),1)
   Q := @
endif

OPTIMIZE_FLAG = -O3
ifeq ($(DEBUG), 1)
   OPTIMIZE_FLAG = -O0
endif

CFLAGS += -Wall $(OPTIMIZE_FLAG) $(INCLUDE_DIRS) -g -I. -pedantic
ifeq ($(CXX_BUILD), 1)
   CFLAGS += -std=c++0x -xc++ -D__STDC_CONSTANT_MACROS
else
ifneq ($(findstring icc,$(CC)),)
   CFLAGS += -std=c99 -D_GNU_SOURCE
else
   CFLAGS += -std=gnu99
endif
endif

ifeq ($(NOUNUSED), yes)
   CFLAGS += -Wno-unused-result
endif

all: $(TARGET) config.mk

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

retroarch: $(OBJ)
	$(Q)$(CXX) -o $@ $(OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
	@$(if $(Q), $(shell echo echo LD $@),)

tools/retroarch-joyconfig: $(JOYCONFIG_OBJ)
ifeq ($(CXX_BUILD), 1)
	$(Q)$(CXX) -o $@ $(JOYCONFIG_OBJ) $(SDL_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
else
	$(Q)$(CC) -o $@ $(JOYCONFIG_OBJ) $(SDL_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
endif
	@$(if $(Q), $(shell echo echo LD $@),)

%.o: %.c config.h config.mk $(HEADERS)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<
	@$(if $(Q), $(shell echo echo CC $<),)

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)/etc 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m644 retroarch.cfg $(DESTDIR)/etc/retroarch.cfg
	install -m644 docs/retroarch.1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m644 docs/retroarch-joyconfig.1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m755 retroarch-zip $(DESTDIR)$(PREFIX)/bin

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-joyconfig
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-zip
	rm -f $(DESTDIR)/etc/retroarch.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch-joyconfig.1

clean:
	rm -f *.o 
	rm -f audio/*.o
	rm -f conf/*.o
	rm -f gfx/*.o
	rm -f gfx/fonts/*.o
	rm -f gfx/context/*.o
	rm -f gfx/py_state/*.o
	rm -f compat/*.o
	rm -f record/*.o
	rm -f input/*.o
	rm -f tools/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
