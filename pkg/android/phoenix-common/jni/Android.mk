LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

RARCH_DIR := ../../../..

HAVE_NEON   := 1
HAVE_LOGGER := 0
HAVE_VULKAN := 1
HAVE_CHEEVOS := 1
HAVE_FILE_LOGGER := 1
HAVE_MENU_WIDGETS := 1

INCFLAGS    :=
DEFINES     :=

LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common
DEPS_DIR          := $(RARCH_DIR)/deps

GIT_VERSION := $(shell git rev-parse --short HEAD 2>/dev/null)
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
   LOCAL_SRC_FILES += $(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float_neon.S.neon \
							 $(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16_neon.S.neon \
							 $(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_neon.S.neon \
							 $(RARCH_DIR)/audio/drivers_resampler/cc_resampler_neon.S.neon
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

#DEFINES += -DHAVE_VIDEO_LAYOUT
DEFINES += -DRARCH_MOBILE -DHAVE_GRIFFIN -DHAVE_STB_VORBIS -DHAVE_LANGEXTRA -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_OVERLAY -DHAVE_VIDEO_LAYOUT -DHAVE_OPENGLES -DGLSL_DEBUG -DHAVE_DYLIB -DHAVE_EGL -DHAVE_GLSL -DHAVE_MENU -DHAVE_RGUI -DHAVE_ZLIB -DHAVE_NO_BUILTINZLIB -DHAVE_RPNG -DHAVE_RJPEG -DHAVE_RBMP -DHAVE_RTGA -DINLINE=inline -DHAVE_THREADS -D__LIBRETRO__ -DHAVE_RSOUND -DHAVE_NETWORKGAMEPAD -DHAVE_NETWORKING -DHAVE_NETPLAYDISCOVERY -DRARCH_INTERNAL -DHAVE_FILTERS_BUILTIN -DHAVE_MATERIALUI -DHAVE_XMB -DHAVE_OZONE -DHAVE_SHADERPIPELINE -DHAVE_LIBRETRODB -DHAVE_STB_FONT -DHAVE_IMAGEVIEWER -DHAVE_ONLINE_UPDATER -DHAVE_UPDATE_ASSETS -DHAVE_UPDATE_CORES -DHAVE_CC_RESAMPLER -DHAVE_MINIUPNPC -DHAVE_BUILTINMINIUPNPC -DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -DHAVE_KEYMAPPER -DHAVE_NETWORKGAMEPAD -DHAVE_FLAC -DHAVE_DR_FLAC -DHAVE_DR_MP3 -DHAVE_CHD -DHAVE_RUNAHEAD -DENABLE_HLSL -DHAVE_EASTEREGG -DHAVE_AUDIOMIXER
DEFINES += -DWANT_IFADDRS
DEFINES += -DHAVE_TRANSLATE

ifeq ($(HAVE_MENU_WIDGETS),1)
DEFINES += -DHAVE_MENU_WIDGETS
endif

ifeq ($(HAVE_VULKAN),1)
DEFINES += -DHAVE_VULKAN -DHAVE_SLANG -DHAVE_GLSLANG -DHAVE_SPIRV_CROSS -DWANT_GLSLANG -D__STDC_LIMIT_MACROS
endif
DEFINES += -DHAVE_7ZIP
DEFINES += -DHAVE_SL

ifeq ($(HAVE_CHEEVOS),1)
DEFINES += -DHAVE_CHEEVOS -DRC_DISABLE_LUA
endif

DEFINES += -DFLAC_PACKAGE_VERSION="\"retroarch\"" -DHAVE_LROUND -DFLAC__HAS_OGG=0

LOCAL_CFLAGS   += -Wall -std=gnu99 -pthread -Wno-unused-function -fno-stack-protector -funroll-loops $(DEFINES)
LOCAL_CPPFLAGS := -fexceptions -fpermissive -std=gnu++11 -fno-rtti -Wno-reorder $(DEFINES)

# Let ndk-build set the optimization flags but remove -O3 like in cf3c3
LOCAL_CFLAGS := $(subst -O3,-O2,$(LOCAL_CFLAGS))

LOCAL_LDLIBS	:= -landroid -lEGL $(GLES_LIB) $(LOGGER_LDLIBS) -ldl
LOCAL_C_INCLUDES := \
							$(LOCAL_PATH)/$(RARCH_DIR)/libretro-common/include/ \
							$(LOCAL_PATH)/$(RARCH_DIR)/deps \
							$(LOCAL_PATH)/$(RARCH_DIR)/deps/stb \
							$(LOCAL_PATH)/$(RARCH_DIR)/deps/7zip

INCLUDE_DIRS     := \
	-I$(LOCAL_PATH)/$(DEPS_DIR)/stb/ \
	-I$(LOCAL_PATH)/$(DEPS_DIR)/7zip/ \
	-I$(LOCAL_PATH)/$(DEPS_DIR)/libFLAC/include

ifeq ($(HAVE_CHEEVOS),1)
INCLUDE_DIRS += -I$(LOCAL_PATH)/$(DEPS_DIR)/rcheevos/include
endif

LOCAL_CFLAGS     += $(INCLUDE_DIRS)
LOCAL_CPPFLAGS   += $(INCLUDE_DIRS)
LOCAL_CXXFLAGS   += $(INCLUDE_DIRS)

ifeq ($(HAVE_VULKAN),1)
INCFLAGS         += $(LOCAL_PATH)/$(RARCH_DIR)/gfx/include
						  
LOCAL_C_INCLUDES += $(INCFLAGS)
LOCAL_CPPFLAGS   += -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang \
						  -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/Public \
						  -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/glslang/MachineIndependent \
						  -I$(LOCAL_PATH)/$(DEPS_DIR)/glslang/glslang/SPIRV \
						  -I$(LOCAL_PATH)/$(DEPS_DIR)/SPIRV-Cross

LOCAL_CFLAGS    += -Wno-sign-compare -Wno-unused-variable -Wno-parentheses
LOCAL_SRC_FILES += $(RARCH_DIR)/griffin/griffin_glslang.cpp
endif

LOCAL_LDLIBS += -lOpenSLES -lz

ifneq ($(SANITIZER),)
   LOCAL_CFLAGS   += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_CPPFLAGS += -g -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LOCAL_LDFLAGS  += -fsanitize=$(SANITIZER)
endif

include $(BUILD_SHARED_LIBRARY)
