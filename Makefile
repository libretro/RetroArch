include config.mk

TARGET = retroarch tools/retroarch-joyconfig tools/retrolaunch/retrolaunch

OBJ = frontend/frontend.o \
		retroarch.o \
		file.o \
		file_path.o \
		hash.o \
		driver.o \
		settings.o \
		dynamic.o \
		dynamic_dummy.o \
		message.o \
		rewind.o \
		gfx/gfx_common.o \
		input/input_common.o \
		input/overlay.o \
		patch.o \
		fifo_buffer.o \
		core_options.o \
		compat/compat.o \
		cheats.o \
		conf/config_file.o \
		screenshot.o \
		gfx/scaler/scaler.o \
		gfx/shader_parse.o \
		gfx/scaler/pixconv.o \
		gfx/scaler/scaler_int.o \
		gfx/scaler/filter.o \
		gfx/image.o \
		gfx/fonts/fonts.o \
		gfx/fonts/bitmapfont.o \
		audio/resampler.o \
		audio/sinc.o \
		performance.o

JOYCONFIG_OBJ = tools/retroarch-joyconfig.o \
	conf/config_file.o \
	file_path.o \
	compat/compat.o \
	tools/input_common_joyconfig.o

RETROLAUNCH_OBJ = tools/retrolaunch/main.o \
	tools/retrolaunch/sha1.o \
	tools/retrolaunch/parser.o \
	tools/retrolaunch/cd_detect.o \
	tools/retrolaunch/rl_fnmatch.o \
	tools/input_common_launch.o \
	file_path.o \
	compat/compat.o \
	conf/config_file.o \
	settings.o

HEADERS = $(wildcard */*.h) $(wildcard *.h)

ifeq ($(findstring Haiku,$(OS)),)
	LIBS = -lm
endif

DEFINES = -DHAVE_CONFIG_H -DHAVE_SCREENSHOTS

ifeq ($(GLOBAL_CONFIG_DIR),)
   GLOBAL_CONFIG_DIR = /etc
endif
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'

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

ifneq ($(findstring BSD,$(OS)),)
   BSD_LOCAL_INC = -I/usr/local/include
endif

ifneq ($(findstring Linux,$(OS)),)
   LIBS += -lrt
   OBJ += input/linuxraw_input.o input/linuxraw_joypad.o
   JOYCONFIG_OBJ += tools/linuxraw_joypad.o
endif

ifeq ($(HAVE_RGUI), 1)
   OBJ += frontend/menu/menu_common.o frontend/menu/rgui.o frontend/menu/history.o
endif

ifeq ($(HAVE_THREADS), 1)
   OBJ += autosave.o thread.o gfx/thread_wrapper.o
   ifeq ($(findstring Haiku,$(OS)),)
      LIBS += -lpthread
   endif
endif

ifeq ($(HAVE_BSV_MOVIE), 1)
   OBJ += movie.o
endif

ifeq ($(HAVE_NETPLAY), 1)
   OBJ += netplay.o
endif

ifeq ($(HAVE_COMMAND), 1)
   OBJ += command.o
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
   OBJ += audio/alsa.o audio/alsathread.o
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

ifeq ($(SCALER_NO_SIMD), 1)
   DEFINES += -DSCALER_NO_SIMD
endif

ifeq ($(PERF_TEST), 1)
   DEFINES += -DPERF_TEST
endif

ifeq ($(HAVE_SDL), 1)
   OBJ += gfx/sdl_gfx.o input/sdl_input.o input/sdl_joypad.o audio/sdl_audio.o
   JOYCONFIG_OBJ += input/sdl_joypad.o
   DEFINES += $(SDL_CFLAGS) $(BSD_LOCAL_INC)
   LIBS += $(SDL_LIBS)

   ifeq ($(HAVE_OPENGL), 1)
      OBJ += gfx/context/sdl_ctx.o
   endif
endif

ifeq ($(HAVE_OPENGL), 1)
   OBJ += gfx/gl.o \
			 gfx/gfx_context.o \
			 gfx/fonts/gl_font.o \
			 gfx/fonts/gl_raster_font.o \
			 gfx/math/matrix.o \
			 gfx/state_tracker.o

   ifeq ($(HAVE_KMS), 1)
      OBJ += gfx/context/drm_egl_ctx.o
      DEFINES += $(GBM_CFLAGS) $(DRM_CFLAGS) $(EGL_CFLAGS)
      LIBS += $(GBM_LIBS) $(DRM_LIBS) $(EGL_LIBS)
   endif

   ifeq ($(HAVE_VIDEOCORE), 1)
      OBJ += gfx/context/vc_egl_ctx.o
		# videocore's libs set later
   endif

   ifeq ($(HAVE_X11), 1)
      ifeq ($(HAVE_GLES), 0)
         OBJ += gfx/context/glx_ctx.o
      endif
      ifeq ($(HAVE_EGL), 1)
         OBJ += gfx/context/xegl_ctx.o
         DEFINES += $(EGL_CFLAGS)
         LIBS += $(EGL_LIBS)
      endif
   endif
	
   ifeq ($(HAVE_GLES), 1)
      LIBS += -lGLESv2
      DEFINES += -DHAVE_OPENGLES -DHAVE_OPENGLES2
   else
      ifeq ($(OSX), 1)
         LIBS += -framework OpenGL
      else
         LIBS += -lGL
         DEFINES += -DHAVE_GL_SYNC
      endif
   endif

   OBJ += gfx/shader_glsl.o 
   DEFINES += -DHAVE_GLSL -DHAVE_OVERLAY
endif

ifeq ($(HAVE_VG), 1)
   OBJ += gfx/vg.o gfx/math/matrix_3x3.o
   DEFINES += $(VG_CFLAGS)
   LIBS += $(VG_LIBS)
endif

ifeq ($(HAVE_VIDEOCORE), 1)
   LIBS += -lbcm_host -lvcos -lvchiq_arm -lEGL
endif

ifeq ($(HAVE_XVIDEO), 1)
   OBJ += gfx/xvideo.o
   LIBS += $(XVIDEO_LIBS) 
   DEFINES += $(XVIDEO_CFLAGS)
endif

ifeq ($(HAVE_X11), 1)
   OBJ += input/x11_input.o gfx/context/x11_common.o
   LIBS += $(X11_LIBS) $(XEXT_LIBS) $(XF86VM_LIBS) $(XINERAMA_LIBS)
   DEFINES += $(X11_CFLAGS) $(XEXT_CFLAGS) $(XF86VM_CFLAGS) $(XINERAMA_CFLAGS)
endif

ifeq ($(HAVE_CG), 1)
   OBJ += gfx/shader_cg.o
   ifeq ($(OSX), 1)
      LIBS += -framework Cg
   else
      LIBS += -lCg -lCgGL
   endif
endif

ifeq ($(HAVE_LIBXML2), 1)
   LIBS += $(LIBXML2_LIBS)
   DEFINES += $(LIBXML2_CFLAGS)
else
   OBJ += compat/rxml/rxml.o
endif

ifeq ($(HAVE_DYLIB), 1)
   LIBS += $(DYLIB_LIB)
endif

ifeq ($(HAVE_FREETYPE), 1)
   OBJ += gfx/fonts/freetype.o
   LIBS += $(FREETYPE_LIBS)
   DEFINES += $(FREETYPE_CFLAGS)
endif

ifeq ($(HAVE_SDL_IMAGE), 1)
   LIBS += $(SDL_IMAGE_LIBS)
   DEFINES += $(SDL_IMAGE_CFLAGS)
endif

ifeq ($(HAVE_ZLIB), 1)
   OBJ += gfx/rpng/rpng.o file_extract.o
   LIBS += $(ZLIB_LIBS)
   DEFINES += $(ZLIB_CFLAGS) -DHAVE_ZLIB_DEFLATE
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

ifeq ($(HAVE_NEON),1)
	OBJ += audio/sinc_neon.o
	# When compiled without this, tries to attempt to compile sinc lerp,
	# which will error out
	DEFINES += -DSINC_LOWER_QUALITY
endif

OBJ += audio/utils.o
ifeq ($(HAVE_NEON),1)
   OBJ += audio/utils_neon.o
endif

ifneq ($(V),1)
   Q := @
endif

OPTIMIZE_FLAG = -O3 -ffast-math
ifeq ($(DEBUG), 1)
   OPTIMIZE_FLAG = -O0
endif

CFLAGS += -Wall $(OPTIMIZE_FLAG) $(INCLUDE_DIRS) -g -I.
ifeq ($(CXX_BUILD), 1)
   LD = $(CXX)
   CFLAGS += -std=c++0x -xc++ -D__STDC_CONSTANT_MACROS
else
   LD = $(CC)
   ifneq ($(findstring icc,$(CC)),)
      CFLAGS += -std=c99 -D_GNU_SOURCE
   else
      CFLAGS += -std=gnu99
   endif
endif

ifeq ($(NOUNUSED), yes)
   CFLAGS += -Wno-unused-result
endif
ifeq ($(NOUNUSED_VARIABLE), yes)
   CFLAGS += -Wno-unused-variable
endif


all: $(TARGET) config.mk

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

retroarch: $(OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LD) -o $@ $(OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

tools/retroarch-joyconfig: $(JOYCONFIG_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
ifeq ($(CXX_BUILD), 1)
	$(Q)$(CXX) -o $@ $(JOYCONFIG_OBJ) $(SDL_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
else
	$(Q)$(CC) -o $@ $(JOYCONFIG_OBJ) $(SDL_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
endif

tools/retrolaunch/retrolaunch: $(RETROLAUNCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LD) -o $@ $(RETROLAUNCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

%.o: %.c config.h config.mk $(HEADERS)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -c -o $@ $<

tools/linuxraw_joypad.o: input/linuxraw_joypad.c
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -DIS_JOYCONFIG -c -o $@ $<

tools/input_common_launch.o: input/input_common.c
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -DIS_RETROLAUNCH -c -o $@ $<

tools/input_common_joyconfig.o: input/input_common.c
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -DIS_JOYCONFIG -c -o $@ $<

%.o: %.S config.h config.mk $(HEADERS)
	@$(if $(Q), $(shell echo echo AS $<),)
	$(Q)$(CC) $(CFLAGS) $(ASFLAGS) $(DEFINES) -c -o $@ $<

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(GLOBAL_CONFIG_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m644 retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	install -m644 docs/retroarch.1 $(DESTDIR)$(MAN_DIR)
	install -m644 docs/retroarch-joyconfig.1 $(DESTDIR)$(MAN_DIR)
	install -m644 media/retroarch.png $(DESTDIR)$(PREFIX)/share/pixmaps

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-joyconfig
	rm -f $(DESTDIR)$(PREFIX)/bin/retrolaunch
	rm -f $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch-joyconfig.1
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/retroarch.png

clean:
	rm -f *.o 
	rm -f frontend/menu/*.o
	rm -f frontend/*.o
	rm -f audio/*.o
	rm -f conf/*.o
	rm -f gfx/*.o
	rm -f gfx/rpng/*.o
	rm -f gfx/fonts/*.o
	rm -f gfx/math/*.o
	rm -f gfx/context/*.o
	rm -f gfx/py_state/*.o
	rm -f gfx/scaler/*.o
	rm -f compat/*.o
	rm -f compat/rxml/*.o
	rm -f record/*.o
	rm -f input/*.o
	rm -f tools/*.o
	rm -f tools/retrolaunch/*.o
	rm -f $(TARGET)

.PHONY: all install uninstall clean
