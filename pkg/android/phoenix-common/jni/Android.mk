LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

RARCH_DIR := ../../../..

HAVE_NEON   := 1
HAVE_LOGGER := 0
HAVE_VULKAN := 1
HAVE_CHEEVOS := 1
HAVE_FILE_LOGGER := 1
HAVE_GFX_WIDGETS := 1
HAVE_SAF := 1
# The cleanroom network stack, as on the desktop builds: the crypto,
# the encrypted keychain, the TLS 1.2/1.3 client and the SMB2/3 client
# with Kerberos. All of it needs nothing
# but sockets; the NFSv3/v4 client and its nfs:// backend likewise.
HAVE_CRYPTO   := 1
HAVE_KEYCHAIN := 1
HAVE_RETROSSL := 1
HAVE_RETROSMB := 1
HAVE_RETRONFS := 1
# The video modeline engine, as on the desktop builds. Android drives
# no modelines (its display server does not advertise DISPSERV_CTX_
# MODELINE, so the CRT SwitchRes menu stays hidden); this is for the
# EDID reader behind Information > Display Information > EDID.
HAVE_MODELINE := 1

INCFLAGS    :=
DEFINES     :=

LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common
DEPS_DIR          := $(RARCH_DIR)/deps

RA_ROOT := $(abspath $(LOCAL_PATH)/$(RARCH_DIR))

# Ask git whether RA_ROOT is itself the repository root. An empty prefix
# means it is; a non-empty one means we resolved an enclosing repository
# and must not use its HEAD. Comparing paths is unreliable here because
# make and git may disagree on path syntax.
ifeq ($(GIT_VERSION),)
GIT_PROBE := $(strip $(shell git -C "$(RA_ROOT)" rev-parse --show-prefix 2>/dev/null && echo GIT_OK))
ifeq ($(GIT_PROBE),GIT_OK)
   GIT_VERSION := $(shell git -C "$(RA_ROOT)" rev-parse --short HEAD 2>/dev/null)
else
ifneq ($(GIT_PROBE),)
   $(warning RetroArch: $(RA_ROOT) is not a git toplevel, omitting git version)
endif
endif
endif

ifneq ($(GIT_VERSION),)
   DEFINES += -DHAVE_GIT_VERSION -DGIT_VERSION=$(GIT_VERSION)
endif

include $(CLEAR_VARS)
ifeq ($(TARGET_ARCH),arm)
   DEFINES += -DANDROID_ARM -marm
   LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
   DEFINES += -DANDROID_X86 -DHAVE_SSSE3
endif

ifeq ($(TARGET_ARCH),x86_64)
   DEFINES += -DANDROID_X64
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)

ifeq ($(HAVE_NEON),1)
	DEFINES += -D__ARM_NEON__ -DHAVE_NEON
endif
DEFINES += -DANDROID_ARM_V7
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
   DEFINES += -DANDROID_AARCH64
endif

ifeq ($(TARGET_ARCH),mips)
   DEFINES += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE := retroarch-activity

LOCAL_SRC_FILES  +=	$(RARCH_DIR)/griffin/griffin.c \
							$(RARCH_DIR)/griffin/griffin_cpp.cpp

ifeq ($(HAVE_CRYPTO),1)
   DEFINES += -DHAVE_CRYPTO
   ifeq ($(HAVE_KEYCHAIN),1)
      DEFINES += -DHAVE_KEYCHAIN
   endif
   ifeq ($(HAVE_RETROSMB),1)
      DEFINES += -DHAVE_SMBCLIENT -DHAVE_RETROSMB
   endif
endif

ifeq ($(HAVE_MODELINE),1)
   DEFINES += -DHAVE_MODELINE
endif

ifeq ($(HAVE_LOGGER), 1)
   DEFINES += -DHAVE_LOGGER
endif
LOGGER_LDLIBS := -llog

ifeq ($(GLES),3)
   GLES_LIB := -lGLESv3
   DEFINES += -DHAVE_OPENGLES3
else
   GLES_LIB := -lGLESv2
   DEFINES += -DHAVE_OPENGLES2
endif

DEFINES += -DRARCH_MOBILE \
	   -DHAVE_GRIFFIN \
	   -DHAVE_RVORBIS \
	   -DHAVE_LANGEXTRA \
	   -DANDROID \
	   -DHAVE_DYNAMIC \
	   -DHAVE_OPENGL \
	   -DHAVE_OVERLAY \
	   -DHAVE_OPENGLES \
	   -DGLSL_DEBUG \
	   -DHAVE_DYLIB \
	   -DHAVE_EGL \
	   -DHAVE_GLSL \
	   -DHAVE_MENU \
	   -DHAVE_CONFIGFILE \
	   -DHAVE_PATCH \
	   -DHAVE_DSP_FILTER \
	   -DHAVE_VIDEO_FILTER \
	   -DHAVE_SCREENSHOTS \
	   -DHAVE_REWIND \
	   -DHAVE_CHEATS \
	   -DHAVE_BSV_MOVIE \
	   -DHAVE_RZSTD \
	   -DZSTD_DISABLE_ASM \
	   -DHAVE_CHEEVOS_RVZ \
	   -DHAVE_RPNG \
	   -DHAVE_RWEBP \
	   -DHAVE_RDDS \
	   -DHAVE_RJPEG \
	   -DHAVE_RBMP \
	   -DHAVE_RTGA \
	   -DINLINE=inline \
	   -DHAVE_THREADS \
	   -DHAVE_THREAD_STORAGE \
	   -D__LIBRETRO__ \
	   -DHAVE_RSOUND \
	   -DHAVE_NETWORKGAMEPAD \
	   -DHAVE_NETWORKING \
	   -DHAVE_NETWORK_CMD \
	   -DHAVE_COMMAND \
	   -DHAVE_CLOUDSYNC \
	   -DHAVE_IFINFO \
	   -DHAVE_NETPLAYDISCOVERY \
	   -DRARCH_INTERNAL \
	   -DHAVE_FILTERS_BUILTIN \
	   -DHAVE_RGUI \
	   -DHAVE_MATERIALUI \
	   -DHAVE_XMB \
	   -DHAVE_OZONE \
	   -DHAVE_SHADERPIPELINE \
	   -DHAVE_LIBRETRODB \
	   -DHAVE_STB_FONT \
	   -DHAVE_IMAGEVIEWER \
	   -DHAVE_ONLINE_UPDATER \
	   -DHAVE_UPDATE_ASSETS \
	   -DHAVE_UPDATE_CORES \
	   -DHAVE_UPDATE_CORE_INFO \
	   -DHAVE_CC_RESAMPLER \
	   -DHAVE_KEYMAPPER \
	   -DHAVE_NETWORKGAMEPAD \
	   -DHAVE_RFLAC \
	   -DHAVE_RMP3 \
	   -DHAVE_CHD \
	   -DWANT_SUBCODE \
	   -DWANT_RAW_DATA_SECTOR \
	   -DHAVE_RUNAHEAD \
	   -DHAVE_AUDIOMIXER \
	   -DHAVE_RWAV \
	   -DHAVE_ACCESSIBILITY \
	   -DHAVE_TRANSLATE \
	   -DWANT_IFADDRS \
	   -DHAVE_XDELTA \
	   -DHAVE_CORE_INFO_CACHE \
	   -DHAVE_SSL

ifeq ($(HAVE_RETROSSL),1)
   DEFINES += -DHAVE_RETROSSL
endif

ifeq ($(HAVE_GFX_WIDGETS),1)
DEFINES += -DHAVE_GFX_WIDGETS
endif

ifeq ($(HAVE_VULKAN),1)
DEFINES += -DHAVE_VULKAN \
	   -DHAVE_SLANG \
	   -DHAVE_GLSLANG \
	   -DHAVE_BUILTINGLSLANG \
	   -DHAVE_SPIRV_CROSS \
	   -DWANT_GLSLANG \
	   -D__STDC_LIMIT_MACROS
endif
DEFINES += -DHAVE_7ZIP \
	   \
	   -DHAVE_SL

ifeq ($(HAVE_CHEEVOS),1)
DEFINES += -DHAVE_CHEEVOS \
	   -DRC_DISABLE_LUA
endif

ifeq ($(HAVE_SAF),1)
   DEFINES += -DHAVE_SAF
endif

ifeq ($(HAVE_RETRONFS),1)
   DEFINES += -DHAVE_NFSCLIENT -DHAVE_RETRONFS
endif

LOCAL_CFLAGS   += -Wall -std=gnu99 -pthread -Wno-unused-function -fno-stack-protector -funroll-loops $(DEFINES)
LOCAL_CPPFLAGS := -fexceptions -fpermissive -std=gnu++11 -fno-rtti -Wno-reorder $(DEFINES)

# Let ndk-build set the optimization flags but remove -O3 like in cf3c3
LOCAL_CFLAGS := $(subst -O3,-O2,$(LOCAL_CFLAGS))

LOCAL_LDLIBS	 := -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl
LOCAL_C_INCLUDES := \
		    $(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include \
		    $(LOCAL_PATH)/$(RARCH_DIR)/deps \
		    $(LOCAL_PATH)/$(RARCH_DIR)/deps/stb

INCLUDE_DIRS     := \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/stb/ \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/7zip/

ifeq ($(HAVE_CHEEVOS),1)
INCLUDE_DIRS += -I$(LOCAL_PATH)/$(DEPS_DIR)/rcheevos/include
endif


LOCAL_CFLAGS     += $(INCLUDE_DIRS)
LOCAL_CPPFLAGS   += $(INCLUDE_DIRS)
LOCAL_CXXFLAGS   += $(INCLUDE_DIRS)

ifeq ($(HAVE_VULKAN),1)
INCFLAGS         += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include

LOCAL_C_INCLUDES += $(INCFLAGS)
# slang_process.c is C and amalgamated into griffin.c; it includes
# <spirv_cross_c.h>, so the SPIRV-Cross directory must be on the C
# include path, not just LOCAL_CPPFLAGS.  LOCAL_C_INCLUDES applies to
# both C and C++ compiles under ndk-build.
LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(DEPS_DIR)/SPIRV-Cross
LOCAL_CPPFLAGS   += -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/Public \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/MachineIndependent \
		    -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/SPIRV

LOCAL_CFLAGS    += -Wno-sign-compare -Wno-unused-variable -Wno-parentheses
LOCAL_SRC_FILES += $(RARCH_DIR)/griffin/griffin_glslang.cpp
endif

LOCAL_LDLIBS += -lOpenSLES

ifneq ($(SANITIZER),)
   LOCAL_CFLAGS   += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_CPPFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_LDFLAGS  += -fsanitize=$(SANITIZER)
endif

ifneq ($(PLAY_STORE_BUILD),1)
   ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
      LOCAL_LDFLAGS += -Wl,-z,max-page-size=4096
   endif

   ifeq ($(TARGET_ARCH_ABI),x86_64)
      LOCAL_LDFLAGS += -Wl,-z,max-page-size=4096
   endif
endif

include $(BUILD_SHARED_LIBRARY)
