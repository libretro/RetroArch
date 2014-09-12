include config.mk

TARGET = retroarch tools/retroarch-joyconfig tools/retrolaunch/retrolaunch

OBJDIR := obj-unix

OBJ = frontend/frontend.o \
		frontend/frontend_context.o \
		frontend/platform/platform_null.o \
		retroarch.o \
		file.o \
		file_list.o \
		file_path.o \
		hash.o \
		driver.o \
		settings.o \
		settings_data.o \
		dynamic.o \
		dynamic_dummy.o \
		message_queue.o \
		rewind.o \
		gfx/gfx_common.o \
		gfx/fonts/bitmapfont.o \
		input/input_common.o \
		input/keyboard_line.o \
		input/overlay.o \
		patch.o \
		fifo_buffer.o \
		core_options.o \
		compat/compat.o \
		cheats.o \
		core_info.o \
		conf/config_file.o \
		screenshot.o \
		gfx/scaler/scaler.o \
		gfx/shader_common.o \
		gfx/shader_parse.o \
		gfx/scaler/pixconv.o \
		gfx/scaler/scaler_int.o \
		gfx/scaler/scaler_filter.o \
		gfx/image/image_rpng.o \
		gfx/fonts/fonts.o \
		audio/resamplers/resampler.o \
		audio/dsp_filter.o \
		audio/resamplers/sinc.o \
		audio/resamplers/nearest.o \
		audio/resamplers/cc_resampler.o \
		location/nulllocation.o \
		camera/nullcamera.o \
		gfx/nullgfx.o \
		audio/nullaudio.o \
		input/nullinput.o \
		input/osk/nullosk.o \
		performance.o


JOYCONFIG_OBJ = tools/retroarch-joyconfig.o \
	conf/config_file.o \
	file_path.o \
	compat/compat.o \
	tools/input_common_joyconfig.o

RETROLAUNCH_OBJ = tools/retrolaunch/main.o \
	hash.o \
	tools/retrolaunch/parser.o \
	tools/retrolaunch/cd_detect.o \
	compat/fnmatch_rarch.o \
	tools/input_common_launch.o \
	file_path.o \
	compat/compat.o \
	conf/config_file.o \
	settings.o

HEADERS = $(wildcard */*/*.h) $(wildcard */*.h) $(wildcard *.h)

ifeq ($(findstring Haiku,$(OS)),)
   LIBS = -lm
   DEBUG_FLAG = -g
else
   LIBS = -lroot -lnetwork
   # stable and nightly haiku builds are stuck on gdb 6.x but we use gcc4
   DEBUG_FLAG = -gdwarf-2
endif

DEFINES = -DHAVE_CONFIG_H -DRARCH_INTERNAL -DHAVE_CC_RESAMPLER -DHAVE_OVERLAY


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
   JOYCONFIG_LIBS += -lrt
   OBJ += input/linuxraw_input.o input/linuxraw_joypad.o
   JOYCONFIG_OBJ += tools/linuxraw_joypad.o
endif

ifeq ($(HAVE_RGUI), 1)
   OBJ += frontend/menu/disp/rgui.o
   DEFINES += -DHAVE_MENU -DHAVE_RGUI
   HAVE_MENU_COMMON = 1
ifeq ($(HAVE_GLUI), 1)
   OBJ += frontend/menu/disp/glui.o
   DEFINES += -DHAVE_GLUI
endif
ifeq ($(HAVE_LAKKA), 1)
   OBJ += frontend/menu/backend/menu_lakka_backend.o frontend/menu/disp/lakka.o
   DEFINES += -DHAVE_LAKKA
endif
endif

OBJ += playlist.o

ifeq ($(HAVE_MENU_COMMON), 1)
   OBJ += frontend/menu/backend/menu_common_backend.o \
			 frontend/menu/menu_input_line_cb.o \
			 frontend/menu/menu_common.o \
			 frontend/menu/menu_navigation.o  \
			 frontend/menu/menu_action.o \
			 frontend/menu/menu_entries.o
endif

ifeq ($(HAVE_THREADS), 1)
   OBJ += autosave.o thread.o gfx/video_thread_wrapper.o audio/thread_wrapper.o
   ifeq ($(findstring Haiku,$(OS)),)
      LIBS += -lpthread
   endif
endif

   OBJ += movie.o

ifeq ($(HAVE_NETPLAY), 1)
   OBJ += netplay.o
   ifneq ($(findstring Win32,$(OS)),)
      LIBS += -lws2_32
   endif
endif

ifeq ($(HAVE_COMMAND), 1)
   OBJ += command.o
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

ifeq ($(HAVE_RSOUND), 1)
   OBJ += audio/rsound.o
   DEFINES += $(RSOUND_CFLAGS)
   LIBS += $(RSOUND_LIBS)
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

ifeq ($(HAVE_HARD_FLOAT), 1)
   DEFINES += -mfloat-abi=hard
endif

ifeq ($(HAVE_AL), 1)
   OBJ += audio/openal.o
   ifeq ($(OSX),1)
      LIBS += -framework OpenAL
   else
      LIBS += -lopenal
   endif
endif

ifeq ($(HAVE_V4L2),1)
   OBJ += camera/video4linux2.o
   DEFINES += -DHAVE_V4L2
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

ifeq ($(HAVE_SDL), 1)
   OBJ += gfx/sdl_gfx.o input/sdl_input.o input/sdl_joypad.o audio/sdl_audio.o

   ifeq ($(HAVE_OPENGL), 1)
     OBJ += gfx/context/sdl_gl_ctx.o
   endif

   JOYCONFIG_OBJ += input/sdl_joypad.o
   JOYCONFIG_LIBS += $(SDL_LIBS)
   DEFINES += $(SDL_CFLAGS) $(BSD_LOCAL_INC)
   LIBS += $(SDL_LIBS)
endif

ifeq ($(HAVE_SDL2), 1)
   OBJ += gfx/sdl2_gfx.o input/sdl_input.o input/sdl_joypad.o audio/sdl_audio.o

   ifeq ($(HAVE_OPENGL), 1)
     OBJ += gfx/context/sdl_gl_ctx.o
   endif

   JOYCONFIG_OBJ += input/sdl_joypad.o
   JOYCONFIG_LIBS += $(SDL2_LIBS)
   DEFINES += $(SDL2_CFLAGS) $(BSD_LOCAL_INC)
   LIBS += $(SDL2_LIBS)
endif

ifeq ($(HAVE_D3D9), 1)
   OBJ += gfx/d3d9/d3d.o gfx/d3d9/render_chain.o gfx/fonts/d3d_font.o gfx/fonts/d3d_w32_font.o gfx/context/d3d_ctx.o
   DEFINES += -DHAVE_WIN32_D3D9
   LIBS += -ld3d9 -ld3dx9 -ldxguid
endif

ifeq ($(HAVE_WINXINPUT), 1)
   OBJ += input/winxinput_joypad.o
   JOYCONFIG_OBJ += input/winxinput_joypad.o
endif

ifeq ($(HAVE_DINPUT), 1)
   LIBS += -ldinput8 -ldxguid -lole32
   OBJ += input/dinput.o
   JOYCONFIG_LIBS += -ldinput8 -ldxguid -lole32
   JOYCONFIG_OBJ += input/dinput.o
endif

ifeq ($(HAVE_XAUDIO), 1)
   OBJ += audio/xaudio.o audio/xaudio-c/xaudio-c.o
   LIBS += -lole32
endif

ifeq ($(HAVE_DSOUND), 1)
   OBJ += audio/dsound.o
   LIBS += -ldxguid -ldsound
endif

ifeq ($(HAVE_OMAP), 1)
   OBJ += gfx/omap_gfx.o
endif

ifeq ($(HAVE_EXYNOS), 1)
   OBJ += gfx/exynos_gfx.o mem/neon/memcpy-neon.o
   LIBS += $(DRM_LIBS) $(EXYNOS_LIBS)
   DEFINES += $(DRM_CFLAGS) $(EXYNOS_CFLAGS)
endif

ifeq ($(HAVE_OPENGL), 1)
   OBJ += gfx/gl.o \
			 gfx/gfx_context.o \
			 gfx/fonts/gl_font.o \
			 gfx/fonts/gl_raster_font.o \
			 gfx/math/matrix.o \
			 gfx/state_tracker.o \
			 gfx/glsym/rglgen.o

   ifeq ($(HAVE_KMS), 1)
      OBJ += gfx/context/drm_egl_ctx.o
      DEFINES += $(GBM_CFLAGS) $(DRM_CFLAGS) $(EGL_CFLAGS)
      LIBS += $(GBM_LIBS) $(DRM_LIBS) $(EGL_LIBS)
   endif

   ifeq ($(HAVE_VIDEOCORE), 1)
      OBJ += gfx/context/vc_egl_ctx.o
      DEFINES += $(EGL_CFLAGS)
      LIBS += $(EGL_LIBS)
   endif

   ifeq ($(HAVE_MALI_FBDEV), 1)
      OBJ += gfx/context/mali_fbdev_ctx.o
   endif

   ifeq ($(HAVE_VIVANTE_FBDEV), 1)
      OBJ += gfx/context/vivante_fbdev_ctx.o
      DEFINES += $(EGL_CFLAGS)
      LIBS += $(EGL_LIBS)
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

   ifeq ($(HAVE_WAYLAND), 1)
	   ifeq ($(HAVE_EGL), 1)
	     OBJ += gfx/context/wayland_ctx.o
      endif
   endif
	
   ifeq ($(HAVE_GLES), 1)
      LIBS += $(GLES_LIBS)
      DEFINES += $(GLES_CFLAGS) -DHAVE_OPENGLES -DHAVE_OPENGLES2
      ifeq ($(HAVE_GLES3), 1)
         DEFINES += -DHAVE_OPENGLES3
      endif
      OBJ += gfx/glsym/glsym_es2.o
   else
      DEFINES += -DHAVE_GL_SYNC
      OBJ += gfx/glsym/glsym_gl.o
      ifeq ($(OSX), 1)
         LIBS += -framework OpenGL
      else ifneq ($(findstring Win32,$(OS)),)
         LIBS += -lopengl32 -lgdi32 -lcomdlg32
         OBJ += gfx/context/wgl_ctx.o gfx/context/win32_common.o
      else
         LIBS += -lGL
      endif
   endif

   OBJ += gfx/shader_glsl.o 
   DEFINES += -DHAVE_GLSL
endif

ifeq ($(HAVE_WAYLAND), 1)
 #OBJ += input/wayland.o
 DEFINES += $(WAYLAND_CFLAGS)
 LIBS += $(WAYLAND_LIBS)
endif

ifeq ($(HAVE_VG), 1)
   OBJ += gfx/vg.o gfx/math/matrix_3x3.o
   DEFINES += $(VG_CFLAGS)
   LIBS += $(VG_LIBS)
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
      ifeq ($(HAVE_D3D9),1)
         LIBS += -lcgD3D9
      endif
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
   OBJ += gfx/filter.o
endif

ifeq ($(HAVE_FREETYPE), 1)
   OBJ += gfx/fonts/freetype.o
   LIBS += $(FREETYPE_LIBS)
   DEFINES += $(FREETYPE_CFLAGS)
endif


ifeq ($(HAVE_ZLIB), 1)
   HAVE_COMPRESSION = 1 
   ZLIB_OBJS =	deps/rzlib/unzip.o \
		deps/rzlib/ioapi.o 
   OBJ += gfx/rpng/rpng.o file_extract.o decompress/zip_support.o
   OBJ += $(ZLIB_OBJS)
   JOYCONFIG_OBJ += decompress/zip_support.o
   JOYCONFIG_OBJ += $(ZLIB_OBJS)
   JOYCONFIG_LIBS += -lz
   RETROLAUNCH_OBJ += decompress/zip_support.o
   RETROLAUNCH_OBJ += $(ZLIB_OBJS)
   LIBS += $(ZLIB_LIBS)
   DEFINES += $(ZLIB_CFLAGS) -DHAVE_ZLIB_DEFLATE -DHAVE_ZLIB
endif

OBJ += record/ffemu.o 

ifeq ($(HAVE_FFMPEG), 1)
   OBJ += record/ffmpeg.o
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

ifeq ($(HAVE_UDEV), 1)
   DEFINES += $(UDEV_CFLAGS)
   LIBS += $(UDEV_LIBS)
   JOYCONFIG_LIBS += $(UDEV_LIBS)
   OBJ += input/udev_input.o input/udev_joypad.o
   JOYCONFIG_OBJ += tools/udev_joypad.o
endif

ifeq ($(HAVE_XKBCOMMON), 1)
   DEFINES += $(XKBCOMMON_CFLAGS)
   LIBS += $(XKBCOMMON_LIBS)
endif

ifeq ($(HAVE_NEON),1)
   OBJ += audio/resamplers/sinc_neon.o
   # When compiled without this, tries to attempt to compile sinc lerp,
   # which will error out
   DEFINES += -DSINC_LOWER_QUALITY
endif

OBJ += audio/utils.o
ifeq ($(HAVE_NEON),1)
   OBJ += audio/utils_neon.o
endif

ifeq ($(HAVE_7ZIP),1)
   HAVE_COMPRESSION = 1 
   DEFINES += -DHAVE_7ZIP
   7ZOBJ = 	deps/7zip/7zIn.o \
		deps/7zip/7zAlloc.o \
		deps/7zip/Bra86.o \
		deps/7zip/CpuArch.o \
		deps/7zip/7zFile.o \
		deps/7zip/7zStream.o \
		deps/7zip/7zBuf2.o \
		deps/7zip/LzmaDec.o \
		deps/7zip/7zCrcOpt.o \
		deps/7zip/Bra.o \
		deps/7zip/7zDec.o \
		deps/7zip/Bcj2.o \
		deps/7zip/7zCrc.o \
		deps/7zip/Lzma2Dec.o \
		deps/7zip/7zBuf.o \
		decompress/7zip_support.o 
   OBJ += $(7ZOBJ)
   RETROLAUNCH_OBJ += $(7ZOBJ)
   JOYCONFIG_OBJ += $(7ZOBJ)
endif

ifdef HAVE_COMPRESSION
   DEFINES += -DHAVE_COMPRESSION
endif 

ifeq ($(HAVE_PRESERVE_DYLIB),1)
   DEFINES += -DNO_DLCLOSE
endif

ifneq ($(V),1)
   Q := @
endif

OPTIMIZE_FLAG = -O3 -ffast-math
ifeq ($(DEBUG), 1)
   OPTIMIZE_FLAG = -O0
endif

ifeq ($(GL_DEBUG), 1)
   CFLAGS += -DGL_DEBUG
   CXXFLAGS += -DGL_DEBUG
endif

CFLAGS += -Wall $(OPTIMIZE_FLAG) $(INCLUDE_DIRS) $(DEBUG_FLAG) -I.
CXXFLAGS = -std=c++0x -xc++ -D__STDC_CONSTANT_MACROS

ifeq ($(CXX_BUILD), 1)
   LINK = $(CXX)
   CFLAGS += $(CXXFLAGS)
else
   ifeq ($(findstring Win32,$(OS)),)
      LINK = $(CC)
   else
      # directx-related code is c++
      LINK = $(CXX)
   endif

   ifneq ($(GNU90_BUILD), 1)
      ifneq ($(findstring icc,$(CC)),)
         CFLAGS += -std=c99 -D_GNU_SOURCE
      else
         CFLAGS += -std=gnu99 -D_GNU_SOURCE
      endif
   endif
endif

ifeq ($(NOUNUSED), yes)
   CFLAGS += -Wno-unused-result
endif
ifeq ($(NOUNUSED_VARIABLE), yes)
   CFLAGS += -Wno-unused-variable
endif

GIT_VERSION := $(shell git rev-parse --short HEAD 2>/dev/null)
ifneq ($(GIT_VERSION),)
   DEFINES += -DHAVE_GIT_VERSION -DGIT_VERSION="\"$(GIT_VERSION)\""
   OBJ += git_version.o
endif

RARCH_OBJ := $(addprefix $(OBJDIR)/,$(OBJ))
RARCH_JOYCONFIG_OBJ := $(addprefix $(OBJDIR)/,$(JOYCONFIG_OBJ))
RARCH_RETROLAUNCH_OBJ := $(addprefix $(OBJDIR)/,$(RETROLAUNCH_OBJ))

all: $(TARGET) config.mk

-include $(RARCH_OBJ:.o=.d) $(RARCH_JOYCONFIG_OBJ:.o=.d) $(RARCH_RETROLAUNCH_OBJ:.o=.d)

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

retroarch: $(RARCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LINK) -o $@ $(RARCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

tools/retroarch-joyconfig: $(RARCH_JOYCONFIG_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
ifeq ($(CXX_BUILD), 1)
	$(Q)$(CXX) -o $@ $(RARCH_JOYCONFIG_OBJ) $(JOYCONFIG_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
else
	$(Q)$(CC) -o $@ $(RARCH_JOYCONFIG_OBJ) $(JOYCONFIG_LIBS) $(LDFLAGS) $(LIBRARY_DIRS)
endif

tools/retrolaunch/retrolaunch: $(RARCH_RETROLAUNCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LINK) -o $@ $(RARCH_RETROLAUNCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

$(OBJDIR)/%.o: %.c config.h config.mk
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CXX $<),)
	$(Q)$(CXX) $(CFLAGS) $(CXXFLAGS) $(DEFINES) -MMD -c -o $@ $<

.FORCE:

$(OBJDIR)/git_version.o: git_version.c .FORCE
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -c -o $@ $<

$(OBJDIR)/tools/linuxraw_joypad.o: input/linuxraw_joypad.c
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -DIS_JOYCONFIG -c -o $@ $<

$(OBJDIR)/tools/udev_joypad.o: input/udev_joypad.c
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -DIS_JOYCONFIG -c -o $@ $<

$(OBJDIR)/tools/input_common_launch.o: input/input_common.c
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -DIS_RETROLAUNCH -c -o $@ $<

$(OBJDIR)/tools/input_common_joyconfig.o: input/input_common.c
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo CC $<),)
	$(Q)$(CC) $(CFLAGS) $(DEFINES) -MMD -DIS_JOYCONFIG -c -o $@ $<

$(OBJDIR)/%.o: %.S config.h config.mk $(HEADERS)
	@mkdir -p $(dir $@)
	@$(if $(Q), $(shell echo echo AS $<),)
	$(Q)$(CC) $(CFLAGS) $(ASFLAGS) $(DEFINES) -c -o $@ $<

install: $(TARGET)
	rm -f $(OBJDIR)/git_version.o
	mkdir -p $(DESTDIR)$(PREFIX)/bin 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(GLOBAL_CONFIG_DIR) 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1 2>/dev/null || /bin/true
	mkdir -p $(DESTDIR)$(PREFIX)/share/pixmaps 2>/dev/null || /bin/true
	install -m755 $(TARGET) $(DESTDIR)$(PREFIX)/bin 
	install -m755 tools/cg2glsl.py $(DESTDIR)$(PREFIX)/bin/retroarch-cg2glsl
	install -m644 retroarch.cfg $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	install -m644 docs/retroarch.1 $(DESTDIR)$(MAN_DIR)
	install -m644 docs/retroarch-cg2glsl.1 $(DESTDIR)$(MAN_DIR)
	install -m644 docs/retroarch-joyconfig.1 $(DESTDIR)$(MAN_DIR)
	install -m644 docs/retrolaunch.1 $(DESTDIR)$(MAN_DIR)
	install -m644 media/retroarch.png $(DESTDIR)$(PREFIX)/share/pixmaps
	install -m644 media/retroarch.svg $(DESTDIR)$(PREFIX)/share/pixmaps

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-joyconfig
	rm -f $(DESTDIR)$(PREFIX)/bin/retroarch-cg2glsl
	rm -f $(DESTDIR)$(PREFIX)/bin/retrolaunch
	rm -f $(DESTDIR)$(GLOBAL_CONFIG_DIR)/retroarch.cfg
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch-cg2glsl.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retroarch-joyconfig.1
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/retrolaunch.1
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/retroarch.png
	rm -f $(DESTDIR)$(PREFIX)/share/pixmaps/retroarch.svg

clean:
	rm -rf $(OBJDIR)
	rm -f $(TARGET)
	rm -f tools/retrolaunch/retrolaunch
	rm -f tools/retroarch-joyconfig

.PHONY: all install uninstall clean
