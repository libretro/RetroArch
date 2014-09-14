include config.mk

TARGET = retroarch tools/retrolaunch/retrolaunch
JTARGET = tools/retroarch-joyconfig 

OBJDIR := obj-unix

ifeq ($(GLOBAL_CONFIG_DIR),)
   GLOBAL_CONFIG_DIR = /etc
endif

OBJ := 
JOYCONFIG_OBJ :=
LIBS :=
DEFINES := -DHAVE_CONFIG_H -DRARCH_INTERNAL -DHAVE_CC_RESAMPLER -DHAVE_OVERLAY
DEFINES += -DGLOBAL_CONFIG_DIR='"$(GLOBAL_CONFIG_DIR)"'

include Makefile.common

JOYCONFIG_OBJ += tools/retroarch-joyconfig.o \
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
   LIBS += -lm
   DEBUG_FLAG = -g
else
   LIBS += -lroot -lnetwork
   # stable and nightly haiku builds are stuck on gdb 6.x but we use gcc4
   DEBUG_FLAG = -gdwarf-2
endif

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

ifeq ($(HAVE_NETPLAY), 1)
   OBJ += netplay.o
   ifneq ($(findstring Win32,$(OS)),)
      LIBS += -lws2_32
   endif
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
         OBJ += gfx/context/wgl_ctx.o \
					 gfx/context/win32_common.o
      else
         LIBS += -lGL
      endif
   endif

   OBJ += gfx/shader_glsl.o 
   DEFINES += -DHAVE_GLSL
endif

ifeq ($(HAVE_DYLIB), 1)
   LIBS += $(DYLIB_LIB)
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

ifneq ($(V),1)
   Q := @
endif

OPTIMIZE_FLAG = -O3 -ffast-math
ifeq ($(DEBUG), 1)
   OPTIMIZE_FLAG = -O0
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

RARCH_OBJ := $(addprefix $(OBJDIR)/,$(OBJ))
RARCH_JOYCONFIG_OBJ := $(addprefix $(OBJDIR)/,$(JOYCONFIG_OBJ))
RARCH_RETROLAUNCH_OBJ := $(addprefix $(OBJDIR)/,$(RETROLAUNCH_OBJ))

all: $(TARGET) $(JTARGET) config.mk

-include $(RARCH_OBJ:.o=.d) $(RARCH_JOYCONFIG_OBJ:.o=.d) $(RARCH_RETROLAUNCH_OBJ:.o=.d)

config.mk: configure qb/*
	@echo "config.mk is outdated or non-existing. Run ./configure again."
	@exit 1

retroarch: $(RARCH_OBJ)
	@$(if $(Q), $(shell echo echo LD $@),)
	$(Q)$(LINK) -o $@ $(RARCH_OBJ) $(LIBS) $(LDFLAGS) $(LIBRARY_DIRS)

$(JTARGET): $(RARCH_JOYCONFIG_OBJ)
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
	rm -f $(JTARGET)

.PHONY: all install uninstall clean
